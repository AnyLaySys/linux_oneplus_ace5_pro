 













#ifndef _SIPA_91XX_H
#define _SIPA_91XX_H

#include "sipa_common.h"
#include <linux/version.h>

#define SIA91XX_ENABLE_LEVEL				(0)
#define SIA91XX_DISABLE_LEVEL				(1)

#define SIPA_ERROR_OK  						(0)
#define SIPA_ERROR_I2C						(1)
#define SIPA_ERROR_BAD_PARAM				(2)
#define SIPA_ERROR_NOCLOCK					(3)
#define SIPA_ERROR_TRIGGER					(4)
#define SIPA_ERROR_I2S_UNMATCH				(5)
#define SIPA_ERROR_SOFT_MUTE				(6)
#define SIPA_ERROR_DEV_START				(7)

#define I2C_RETRIES 						(5)
#define I2C_RETRY_DELAY 					(5) 		 

#define TRIGGER_RISING						(0)			 
#define TRIGGER_FALLING						(1)			 
#define MUTE_RETRIES						(10)
#define SIA91XX_MAX_PA_START_TRY_COUNT		(5)

#define CHANNEL_NUM_MIN						(1)
#define CHANNEL_NUM_MAX						(8)

#define SIA91XX_REG_INT_STAT				(0x08)
#define SIA91XX_REG_INT_WC_REG				(0x09)
#define SIA91XX_REG_INT_REG					(0x0A)
#define SIA91XX_REG_INT_EN					(0x11)
#define SIA91XX_REG_EFUS_ETC				(0x23)
#define SIA91XX_REG_IVADC_CONF2				(0x2b)
#define SIA91XX_REG_FLASH 					(0x6f)


#define SIA91XX_RATES 						(SNDRV_PCM_RATE_8000_48000)
#define SIA91XX_FORMATS						((SNDRV_PCM_FMTBIT_S16_LE) | \
											(SNDRV_PCM_FMTBIT_S24_LE) | \
											(SNDRV_PCM_FMTBIT_S32_LE))

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
 
int sia91xx_check_status_reg(sipa_dev_t *si_pa);
#endif

int sia91xx_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *dai);
int sia91xx_hw_params(struct snd_pcm_substream *substream,
struct snd_pcm_hw_params *params, struct snd_soc_dai *dai);
int sia91xx_mute(struct snd_soc_dai *dai, int mute, int stream);
int sia91xx_ext_reset(sipa_dev_t *si_pa);
void sia91xx_interrupt(struct work_struct *work);
int sia91xx_codec_probe(struct snd_soc_codec *codec);
int sia91xx_component_probe(struct snd_soc_component *component);
int sia91xx_detect_chip(sipa_dev_t *si_pa);
int sia91xx_set_fmt(struct snd_soc_dai *dai, unsigned int fmt);
int sia91xx_append_i2c_address(
struct device *dev, struct i2c_client *i2c, struct snd_soc_dapm_widget *widgets,
int num_widgets, struct snd_soc_dai_driver *dai_drv, int num_dai);
int sia91xx_smartpa_start(sipa_dev_t *si_pa, int stream);
int sia91xx_smartpa_soft_mute(sipa_dev_t *si_pa);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0))
int sipa_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id);
#else
int sipa_i2c_probe(struct i2c_client *i2c);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
int sipa_i2c_remove(struct i2c_client *i2c);
#else
void sipa_i2c_remove(struct i2c_client *i2c);
#endif
#endif  
