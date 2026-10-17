#include <linux/device.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/soc/qcom/pmic_glink.h>
#include <linux/soc/qcom/qti_pmic_glink.h>

struct qti_pmic_glink_handle {
	struct pmic_glink_client *client;
	struct pmic_glink_client_data data;
};

static void qti_pmic_glink_message(const void *data, size_t len, void *priv)
{
	struct qti_pmic_glink_handle *handle = priv;

	if (handle->data.msg_cb)
		handle->data.msg_cb(handle->data.priv, (void *)data, len);
}

static void qti_pmic_glink_state(void *priv, int state)
{
	struct qti_pmic_glink_handle *handle = priv;

	if (handle->data.state_cb)
		handle->data.state_cb(handle->data.priv, state ?
				      PMIC_GLINK_STATE_UP : PMIC_GLINK_STATE_DOWN);
}

struct pmic_glink_client *pmic_glink_register_client(struct device *dev,
						     const struct pmic_glink_client_data *data)
{
	struct qti_pmic_glink_handle *handle;

	handle = devm_kzalloc(dev, sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return ERR_PTR(-ENOMEM);

	handle->data = *data;
	handle->client = devm_pmic_glink_client_alloc_global(dev, data->id,
							     qti_pmic_glink_message,
							     qti_pmic_glink_state,
							     handle);
	if (IS_ERR(handle->client))
		return handle->client;

	pmic_glink_client_register(handle->client);
	return (struct pmic_glink_client *)handle;
}
EXPORT_SYMBOL_GPL(pmic_glink_register_client);

int pmic_glink_unregister_client(struct pmic_glink_client *client)
{
	struct qti_pmic_glink_handle *handle = (struct qti_pmic_glink_handle *)client;

	handle->data.msg_cb = NULL;
	handle->data.state_cb = NULL;
	return 0;
}
EXPORT_SYMBOL_GPL(pmic_glink_unregister_client);

int pmic_glink_write(struct pmic_glink_client *client, void *data, size_t len)
{
	struct qti_pmic_glink_handle *handle = (struct qti_pmic_glink_handle *)client;

	return pmic_glink_send(handle->client, data, len);
}
EXPORT_SYMBOL_GPL(pmic_glink_write);

MODULE_LICENSE("GPL");
