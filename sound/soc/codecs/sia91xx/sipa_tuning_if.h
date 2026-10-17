 













#ifndef _SIPA_TUNING_IF_H
#define _SIPA_TUNING_IF_H

#include <linux/fs.h>

struct sipa_cal_opt {
	int (*init)(void);
	void (*exit)(void);
	unsigned long (*open)(uint32_t cal_id);
	int (*close)(unsigned long handle);
	int (*read)(unsigned long handle, uint32_t mode_id, uint32_t param_id,
		uint32_t size, uint8_t *payload);
	int (*write)(unsigned long  handle, uint32_t mode_id, uint32_t param_id,
		uint32_t size, uint8_t *payload);
};

#define DATA_MAX_LEN    4096
#define MSG_MAX_SIZE    128

typedef struct dev_comm_data {
	uint32_t opt; 
	uint32_t param_id; 
	uint32_t payload_size;
	uint32_t reserve;
	uint8_t payload[];
} __packed dev_comm_data_t;

#define DEV_COMM_DATA_LEN(data) \
	((uint32_t)sizeof(struct dev_comm_data) + data->payload_size)

#define PARAM_CHECK(buf, len) \
	if (buf == NULL || len <= 0 || len > DATA_MAX_LEN) { \
		pr_err("%s: param invalid \n", __func__);    \
		return -EFAULT;                              \
	}

typedef struct {
	wait_queue_head_t wq;
	uint8_t data[DATA_MAX_LEN];
	uint32_t len;
	bool flag;
} sipa_sync_t;

typedef struct {
	sipa_sync_t toolup;
	sipa_sync_t tooldown;
	sipa_sync_t cmdup;
	sipa_sync_t cmddown;
	struct mutex lock;
} sipa_turning_t;

enum cal_packet_opt {
	OPT_SET_CAL_VAL,
	OPT_GET_CAL_VAL,
	OPT_ADD_CAL_ID,
	OPT_REM_CAL_ID,
	OPT_GET_CAL_VAL_BY_ADB,
};


#define BOX_NAME_MAX_LEN 50
typedef struct {
	uint8_t len;
	char    boxname[BOX_NAME_MAX_LEN];
} box_name_t;

typedef struct {
	uint8_t pa_idx;    
	uint32_t scene;   
} scene_data_t;

#define SIPA_IOCTL_LOAD_FIRMWARE     _IOW(0x10, 0xA0, box_name_t)
#define SIPA_IOCTL_POWER_ON          _IOW(0x10, 0xA1, scene_data_t)
#define SIPA_IOCTL_POWER_OFF         _IOW(0x10, 0xA2, scene_data_t)
#define SIPA_IOCTL_GET_CHANNEL       _IOW(0x10, 0xA3, int)
#define SIPA_IOCTL_REG_DUMP          _IOW(0x10, 0xA4, int)

#define SIPA_TUNING_CTRL_WR_UP       _IOW(0x10, 0xE0, dev_comm_data_t)
#define SIPA_TUNING_CTRL_RD_UP       _IOR(0x10, 0xE1, dev_comm_data_t)
#define SIPA_TUNING_CTRL_WR_DOWN     _IOW(0x10, 0xE2, dev_comm_data_t)
#define SIPA_TUNING_CTRL_RD_DOWN     _IOR(0x10, 0xE3, dev_comm_data_t)

extern struct file_operations sipa_turning_cmd_fops;
extern struct file_operations sipa_turning_tool_fops;
extern sipa_turning_t *g_sipa_turning;

#endif  
