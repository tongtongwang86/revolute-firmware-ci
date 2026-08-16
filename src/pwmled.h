#ifndef PWM_LED_H
#define PWM_LED_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include "power.h"
#include "ble.h"


// extern led_state_t target_state;


// Initialize the PWM LED system
int pwmled_init(void);

/* Fade out, stop the animation thread and park the LED pin. Must be called
 * before sys_poweroff(): the nRF52 retains GPIO output state through System OFF,
 * so a duty cycle left set here keeps the LED lit for the whole time the device
 * is meant to be off. */
void pwmled_shutdown(void);


#endif // PWM_LED_H