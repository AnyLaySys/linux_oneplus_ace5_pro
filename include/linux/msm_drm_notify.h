#ifndef _LINUX_MSM_DRM_NOTIFY_H
#define _LINUX_MSM_DRM_NOTIFY_H

#include <linux/notifier.h>

struct msm_drm_notifier {
	int id;
	void *data;
};

#define MSM_DRM_EARLY_EVENT_BLANK 0x10
#define MSM_DRM_BLANK_UNBLANK 0
#define MSM_DRM_BLANK_POWERDOWN 4

int msm_drm_register_client(struct notifier_block *nb);
int msm_drm_unregister_client(struct notifier_block *nb);

#endif
