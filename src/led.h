#ifndef LED_H
#define LED_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/types.h>

int led_init(void);

// Function to trigger the LED notify work
void led_notify_trigger(void);

#endif // LED_H

