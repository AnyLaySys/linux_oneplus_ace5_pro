#ifndef _OPLUS_DEVICE_INFO_H
#define _OPLUS_DEVICE_INFO_H

#define INFO_LEN 24

struct manufacture_info {
	char name[INFO_LEN];
	char *version;
	char *manufacture;
	char *fw_path;
};

int register_device_proc(char *name, char *version, char *vendor);
int register_devinfo(char *name, struct manufacture_info *info);

#endif
