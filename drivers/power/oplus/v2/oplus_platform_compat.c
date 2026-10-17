#include <linux/of.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/msm_drm_notify.h>
#include <linux/notifier.h>
#include <soc/oplus/device_info.h>
#include <soc/oplus/system/boot_mode.h>
#include <soc/oplus/system/kernel_fb.h>
#include <soc/oplus/system/oplus_project.h>

int get_boot_mode(void)
{
	return MSM_BOOT_MODE__NORMAL;
}

unsigned int get_project(void)
{
	struct device_node *node;
	u32 project = 0;

	node = of_find_compatible_node(NULL, NULL, "oplus,hal_sm8350");
	if (node) {
		of_property_read_u32(node, "oplus,project-id", &project);
		of_node_put(node);
	}

	return project;
}

unsigned int get_prj(void)
{
	return get_project();
}

unsigned int get_eng_version(void)
{
	return OEM_RELEASE;
}

unsigned int get_PCB_Version(void)
{
	return MP1;
}

int32_t get_Operator_Version(void)
{
	return 0;
}

int register_device_proc(char *name, char *version, char *vendor)
{
	return 0;
}

int register_devinfo(char *name, struct manufacture_info *info)
{
	return 0;
}

int fb_kevent_send_to_user(void *data)
{
	return 0;
}

int msm_drm_register_client(struct notifier_block *nb)
{
	return -ENODEV;
}

int msm_drm_unregister_client(struct notifier_block *nb)
{
	return 0;
}

