#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/unaligned.h>

#define S3910_MAX_PAYLOAD 4096
#define S3910_MAX_OBJECTS 10
#define S3910_MARKER 0xa5
#define S3910_PADDING 0x5a
#define S3910_STATUS_OK 0x01
#define S3910_CONTINUED_READ 0x03
#define S3910_IDENTIFY 0x02
#define S3910_ENABLE_REPORT 0x05
#define S3910_DISABLE_REPORT 0x06
#define S3910_GET_APP_INFO 0x20
#define S3910_SET_DYNAMIC_CONFIG 0x24
#define S3910_GET_REPORT_CONFIG 0x25
#define S3910_SET_REPORT_CONFIG 0x26
#define S3910_ENTER_SLEEP 0x2c
#define S3910_EXIT_SLEEP 0x2d
#define S3910_REPORT_IDENTIFY 0x10
#define S3910_REPORT_TOUCH 0x11
#define S3910_REPORT_HBP 0x23
#define S3910_CONFIG_LBP_HBP 0x11

struct s3910 {
	struct spi_device *spi;
	struct input_dev *input;
	struct touchscreen_properties props;
	struct gpio_desc *reset;
	struct regulator_bulk_data supplies[2];
	u8 *tx;
	u8 *rx;
	u16 length;
	u16 max_write;
	u16 config_size;
	u16 max_x;
	u16 max_y;
	u16 objects;
};

static const u8 s3910_report_config[] = {
	0x18, 8,
	0x01,
	0x06, 8,
	0x07, 8,
	0x08, 16,
	0x09, 16,
	0x03,
	0x00,
};

static int s3910_read(struct s3910 *ts, size_t length)
{
	struct spi_transfer transfer = {
		.tx_buf = ts->tx,
		.rx_buf = ts->rx,
		.len = length,
	};

	memset(ts->tx, 0xff, length);
	return spi_sync_transfer(ts->spi, &transfer, 1);
}

static int s3910_receive(struct s3910 *ts)
{
	int error, code;

	ts->length = 0;
	error = s3910_read(ts, 4);
	if (error)
		return error;
	if (ts->rx[0] != S3910_MARKER)
		return -EPROTO;

	code = ts->rx[1];
	if (!code || code == 0xff)
		return -EAGAIN;
	if (code == S3910_CONTINUED_READ)
		return -EPROTO;

	ts->length = get_unaligned_le16(ts->rx + 2);
	if (ts->length > S3910_MAX_PAYLOAD)
		return -EMSGSIZE;
	if (!ts->length)
		return code;

	error = s3910_read(ts, ts->length + 3);
	if (error)
		return error;
	if (ts->rx[0] != S3910_MARKER ||
	    ts->rx[1] != S3910_CONTINUED_READ ||
	    ts->rx[ts->length + 2] != S3910_PADDING)
		return -EPROTO;

	return code;
}

static int s3910_command(struct s3910 *ts, u8 command,
			 const void *payload, size_t length)
{
	unsigned long deadline;
	int error, code;

	if (length + 3 > ts->max_write)
		return -EMSGSIZE;

	ts->tx[0] = command;
	put_unaligned_le16(length, ts->tx + 1);
	if (length)
		memcpy(ts->tx + 3, payload, length);
	error = spi_write(ts->spi, ts->tx, length + 3);
	if (error)
		goto fail;

	deadline = jiffies + msecs_to_jiffies(1000);
	do {
		usleep_range(5000, 6000);
		code = s3910_receive(ts);
		if (code == S3910_STATUS_OK ||
		    (command == S3910_IDENTIFY && code == S3910_REPORT_IDENTIFY))
			return 0;
		if (code == -EAGAIN || code >= S3910_REPORT_IDENTIFY)
			continue;
		error = code < 0 ? code : -EIO;
		dev_err(&ts->spi->dev, "Command %02x response %d\n", command, code);
		goto fail;
	} while (time_before(jiffies, deadline));
	error = -ETIMEDOUT;
fail:
	return dev_err_probe(&ts->spi->dev, error, "Command %02x failed\n", command);
}

static int s3910_identify(struct s3910 *ts)
{
	const u8 *data = ts->rx + 2;
	int error;

	gpiod_set_value_cansleep(ts->reset, 1);
	msleep(10);
	gpiod_set_value_cansleep(ts->reset, 0);
	msleep(200);

	error = s3910_receive(ts);
	if (error != S3910_REPORT_IDENTIFY) {
		error = s3910_command(ts, S3910_IDENTIFY, NULL, 0);
		if (error)
			return error;
	}
	if (ts->length < 24 || memcmp(data + 2, "S3910", 5))
		return -ENODEV;
	if (data[1] != 1)
		return dev_err_probe(&ts->spi->dev, -ENODEV,
				     "Firmware is not in application mode: %02x\n", data[1]);

	if (get_unaligned_le16(data + 22))
		ts->max_write = min(ts->max_write, get_unaligned_le16(data + 22));
	dev_info(&ts->spi->dev, "TCM1 %.16s firmware %u\n",
		 data + 2, get_unaligned_le32(data + 18));
	return 0;
}

static int s3910_configure(struct s3910 *ts)
{
	u8 mode[] = { S3910_CONFIG_LBP_HBP, 2, 0 };
	u8 report = S3910_REPORT_HBP;
	u8 *config;
	int error;

	error = s3910_command(ts, S3910_DISABLE_REPORT, &report, 1);
	if (error)
		return error;
	report = S3910_REPORT_TOUCH;
	error = s3910_command(ts, S3910_DISABLE_REPORT, &report, 1);
	if (error)
		return error;
	error = s3910_command(ts, S3910_SET_DYNAMIC_CONFIG, mode, sizeof(mode));
	if (error)
		return error;

	config = kzalloc(ts->config_size, GFP_KERNEL);
	if (!config)
		return -ENOMEM;
	memcpy(config, s3910_report_config, sizeof(s3910_report_config));
	error = s3910_command(ts, S3910_SET_REPORT_CONFIG, config, ts->config_size);
	kfree(config);
	if (error)
		return error;
	error = s3910_command(ts, S3910_GET_REPORT_CONFIG, NULL, 0);
	if (error)
		return error;
	if (ts->length < sizeof(s3910_report_config) ||
	    memcmp(ts->rx + 2, s3910_report_config, sizeof(s3910_report_config)))
		return -EPROTO;

	error = s3910_command(ts, S3910_EXIT_SLEEP, NULL, 0);
	if (error)
		return error;
	return s3910_command(ts, S3910_ENABLE_REPORT, &report, 1);
}

static int s3910_report(struct s3910 *ts)
{
	const u8 *data = ts->rx + 2;
	unsigned long seen = 0;
	unsigned int i, count;

	if (!ts->length)
		return -EPROTO;
	count = data[0];
	if (count > ts->objects || ts->length != 1 + count * 6)
		return -EPROTO;

	for (i = 0; i < count; i++) {
		const u8 *point = data + 1 + i * 6;

		if (point[0] >= ts->objects || (seen & BIT(point[0])) ||
		    get_unaligned_le16(point + 2) > ts->max_x ||
		    get_unaligned_le16(point + 4) > ts->max_y)
			return -EPROTO;
		seen |= BIT(point[0]);
	}
	for (i = 0; i < count; i++) {
		const u8 *point = data + 1 + i * 6;
		bool active = point[1] == 1 || point[1] == 2;

		input_mt_slot(ts->input, point[0]);
		input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, active);
		if (active)
			touchscreen_report_pos(ts->input, &ts->props,
					       get_unaligned_le16(point + 2),
					       get_unaligned_le16(point + 4), true);
	}
	input_mt_sync_frame(ts->input);
	input_sync(ts->input);
	return 0;
}

static irqreturn_t s3910_irq(int irq, void *data)
{
	struct s3910 *ts = data;
	int error;

	error = s3910_receive(ts);
	if (error == S3910_REPORT_TOUCH)
		error = s3910_report(ts);
	else if (error == S3910_REPORT_IDENTIFY) {
		input_mt_sync_frame(ts->input);
		input_sync(ts->input);
		error = s3910_configure(ts);
	}
	if (error < 0 && error != -EAGAIN) {
		dev_err_ratelimited(&ts->spi->dev, "Touch report failed: %d\n", error);
		msleep(20);
	}
	return IRQ_HANDLED;
}

static void s3910_power_off(void *data)
{
	struct s3910 *ts = data;

	gpiod_set_value_cansleep(ts->reset, 1);
	regulator_bulk_disable(ARRAY_SIZE(ts->supplies), ts->supplies);
}

static int s3910_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct s3910 *ts;
	const u8 *data;
	int error;

	if (spi->irq <= 0)
		return dev_err_probe(dev, -EINVAL, "Missing interrupt\n");
	ts = devm_kzalloc(dev, sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;
	ts->spi = spi;
	spi_set_drvdata(spi, ts);
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	error = spi_setup(spi);
	if (error)
		return error;
	ts->max_write = min_t(size_t, 1024, spi_max_transfer_size(spi));
	if (spi_max_transfer_size(spi) < S3910_MAX_PAYLOAD + 3)
		return -EMSGSIZE;
	ts->tx = devm_kmalloc(dev, S3910_MAX_PAYLOAD + 3, GFP_KERNEL);
	ts->rx = devm_kmalloc(dev, S3910_MAX_PAYLOAD + 3, GFP_KERNEL);
	if (!ts->tx || !ts->rx)
		return -ENOMEM;
	ts->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ts->reset))
		return dev_err_probe(dev, PTR_ERR(ts->reset), "Reset GPIO unavailable\n");
	ts->supplies[0].supply = "vddio";
	ts->supplies[1].supply = "avdd";
	error = devm_regulator_bulk_get(dev, ARRAY_SIZE(ts->supplies), ts->supplies);
	if (error)
		return dev_err_probe(dev, error, "Touch supplies unavailable\n");
	error = regulator_bulk_enable(ARRAY_SIZE(ts->supplies), ts->supplies);
	if (error)
		return error;
	error = devm_add_action_or_reset(dev, s3910_power_off, ts);
	if (error)
		return error;
	msleep(200);
	error = s3910_identify(ts);
	if (error)
		return dev_err_probe(dev, error, "Identification failed\n");
	error = s3910_command(ts, S3910_GET_APP_INFO, NULL, 0);
	if (error)
		return error;
	data = ts->rx + 2;
	if (ts->length < 38 || get_unaligned_le16(data + 2))
		return dev_err_probe(dev, -ENODEV, "Application firmware not ready\n");
	ts->config_size = get_unaligned_le16(data + 12);
	ts->max_x = get_unaligned_le16(data + 32);
	ts->max_y = get_unaligned_le16(data + 34);
	ts->objects = get_unaligned_le16(data + 36);
	if (ts->config_size < sizeof(s3910_report_config) ||
	    ts->config_size + 3 > ts->max_write ||
	    !ts->max_x || !ts->max_y || !ts->objects ||
	    ts->objects > S3910_MAX_OBJECTS)
		return dev_err_probe(dev, -EINVAL, "Unsupported application limits\n");
	error = s3910_configure(ts);
	if (error)
		return dev_err_probe(dev, error, "LBP configuration failed\n");

	ts->input = devm_input_allocate_device(dev);
	if (!ts->input)
		return -ENOMEM;
	ts->input->name = "Synaptics S3910";
	ts->input->id.bustype = BUS_SPI;
	input_set_abs_params(ts->input, ABS_MT_POSITION_X, 0, ts->max_x, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_POSITION_Y, 0, ts->max_y, 0, 0);
	touchscreen_parse_properties(ts->input, true, &ts->props);
	error = input_mt_init_slots(ts->input, ts->objects,
				    INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);
	if (error)
		return error;
	error = input_register_device(ts->input);
	if (error)
		return error;
	error = devm_request_threaded_irq(dev, spi->irq, NULL, s3910_irq,
					 IRQF_ONESHOT, dev_name(dev), ts);
	if (error)
		return dev_err_probe(dev, error, "Touch interrupt unavailable\n");
	dev_info(dev, "LBP touch ready: %ux%u, %u contacts\n",
		 ts->max_x, ts->max_y, ts->objects);
	return 0;
}

static int s3910_suspend(struct device *dev)
{
	struct s3910 *ts = dev_get_drvdata(dev);
	int error;

	disable_irq(ts->spi->irq);
	input_mt_sync_frame(ts->input);
	input_sync(ts->input);
	error = s3910_command(ts, S3910_ENTER_SLEEP, NULL, 0);
	if (error)
		enable_irq(ts->spi->irq);
	return error;
}

static int s3910_resume(struct device *dev)
{
	struct s3910 *ts = dev_get_drvdata(dev);
	int error;

	error = s3910_identify(ts);
	if (!error)
		error = s3910_configure(ts);
	enable_irq(ts->spi->irq);
	return error;
}

static DEFINE_SIMPLE_DEV_PM_OPS(s3910_pm_ops, s3910_suspend, s3910_resume);

static const struct of_device_id s3910_of_match[] = {
	{ .compatible = "synaptics,s3910" },
	{ }
};
MODULE_DEVICE_TABLE(of, s3910_of_match);

static const struct spi_device_id s3910_ids[] = {
	{ "s3910" },
	{ }
};
MODULE_DEVICE_TABLE(spi, s3910_ids);

static struct spi_driver s3910_driver = {
	.driver = {
		.name = "synaptics-s3910",
		.of_match_table = s3910_of_match,
		.pm = pm_sleep_ptr(&s3910_pm_ops),
	},
	.probe = s3910_probe,
	.id_table = s3910_ids,
};
module_spi_driver(s3910_driver);

MODULE_DESCRIPTION("Synaptics S3910 TouchComm SPI driver");
MODULE_LICENSE("GPL");
