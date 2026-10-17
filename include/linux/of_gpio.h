#ifndef _LINUX_OF_GPIO_H
#define _LINUX_OF_GPIO_H

#include <linux/of.h>
#include <linux/gpio.h>

int of_get_named_gpio(const struct device_node *np, const char *propname,
		      int index);

#endif
