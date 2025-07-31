#ifndef PWM_LED_H
#define PWM_LED_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include "power.h"
#include "ble.h"


extern led_state_t target_state;


// Initialize the PWM LED system
int pwmled_init(void);


#endif // PWM_LED_H