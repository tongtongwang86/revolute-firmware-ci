#ifndef PWM_LED_H
#define PWM_LED_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>

// LED states
typedef enum {
    STATE_OFF,
    STATE_PAIRING,
    STATE_ADVERTISEMENT,
    STATE_CONNECTED
} led_state_t;

// Initialize the PWM LED system
int pwmled_init(void);

// Set the LED state
void set_led_state(led_state_t state);

#endif // PWM_LED_H