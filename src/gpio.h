#ifndef GPIO_H
#define GPIO_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)
#define SW2_NODE DT_ALIAS(sw2)
#define SW3_NODE DT_ALIAS(sw3)
#define LED0_NODE DT_ALIAS(led0)

// Declare the led variable as extern so it can be accessed in other files
extern const struct gpio_dt_spec led;

int ledInit(void);

#endif // GPIO_H
