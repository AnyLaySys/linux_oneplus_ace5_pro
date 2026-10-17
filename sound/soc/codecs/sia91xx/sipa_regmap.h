 













#ifndef _SIPA_REGMAP_H
#define _SIPA_REGMAP_H

#include <linux/regmap.h>

 








static inline uint8_t crc4_itu(uint8_t *data, uint32_t length)
{
	uint8_t i;
	uint8_t crc = 0;						 

	while (length--) {
		crc ^= *data++;						 
		for (i = 0; i < 8; ++i)	{
			if (crc & 1)
				crc = (crc >> 1) ^ 0x0C;	 
			else
				crc = (crc >> 1);
		}
	}

	return crc;
}

 








static inline uint8_t crc8_maxim(uint8_t *data, uint32_t length)
{
	uint8_t i;
	uint8_t crc = 0;						 

	while (length--) {
		crc ^= *data++;						 
		for (i = 0; i < 8; i++) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0x8C;	 
			else
				crc >>= 1;
		}
	}

	return crc;
}

 








static inline uint16_t crc16_maxim(uint8_t *data, uint32_t length)
{
	uint8_t i;
	uint16_t crc = 0;						 

	while (length--) {
		crc ^= *data++;						 
		for (i = 0; i < 8; ++i) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0xA001;	 
			else
				crc = (crc >> 1);
		}
	}

	return ~crc;							 
}

#ifdef PLATFORM_TYPE_QCOM
typedef  unsigned short uint16_t;
typedef  unsigned int   uint32_t;
typedef  unsigned char  uint8_t;
#endif

int sipa_regmap_read(struct regmap *regmap, unsigned int chip_type,
	unsigned int start_reg, unsigned int reg_num, void *buf);
int sipa_regmap_write(struct regmap *regmap, unsigned int chip_type,
	unsigned int start_reg, unsigned int reg_num, const void *buf);

struct regmap *sipa_regmap_init(struct i2c_client *client,
	unsigned int ch, unsigned int chip_type);

 
int sipa_regmap_check_chip_id(struct regmap *regmap,
	unsigned int ch, unsigned int chip_type);
void sipa_regmap_defaults(struct regmap *regmap,
	unsigned int chip_type, unsigned int scene, unsigned int channel_num);
bool sipa_regmap_set_chip_on(sipa_dev_t *si_pa);
bool sipa_regmap_set_chip_off(sipa_dev_t *si_pa);
bool sipa_regmap_get_chip_en(sipa_dev_t *si_pa);

void sipa_regmap_set_pvdd_limit(
	struct regmap *regmap, uint32_t chip_type, uint32_t ch, unsigned int vol);

void sipa_regmap_check_trimming(sipa_dev_t *si_pa);

int sipa_read_reg(struct regmap *regmap, unsigned char subaddress,
   unsigned int *val);
int sipa_write_reg(struct regmap *regmap, unsigned char subaddr,
	unsigned int val);
int sia91xx_write_reg16_bit_pos(sipa_dev_t *si_pa,
	uint8_t subaddr, uint16_t Val, uint8_t pos, uint8_t len);
int sia91xx_write_reg16_bit_msk(sipa_dev_t *si_pa,
	uint8_t subaddr, uint16_t msk, bool stat);

int sipa_reg_init(struct sipa_dev_s *si_pa);
bool sipa_regmap_write_sram(sipa_dev_t *si_pa);

#endif  

