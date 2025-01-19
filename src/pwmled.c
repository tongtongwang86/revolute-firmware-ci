#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include "pwmled.h"

#define NUM_STEPS   50U
#define SLEEP_MSEC  25U

#define PWM_LED0    DT_ALIAS(pwm_led0)
static const struct pwm_dt_spec pwm_led0 = PWM_DT_SPEC_GET(PWM_LED0);

#define PWMLED_STACK_SIZE 1024
#define PWMLED_THREAD_PRIORITY K_LOWEST_APPLICATION_THREAD_PRIO

static struct k_thread pwmled_thread_data;
static K_THREAD_STACK_DEFINE(pwmled_stack, PWMLED_STACK_SIZE);

// Define current and target LED state
static led_state_t current_state = STATE_OFF;
static led_state_t target_state = STATE_OFF;
static uint32_t pulse_width = 0;  // Track the current LED brightness

// Function to set LED pulse width
static void set_led_pulse(uint32_t pulse_width) {
    int ret = pwm_set_pulse_dt(&pwm_led0, pulse_width);
    if (ret) {
        printk("Error %d: failed to set pulse width\n", ret);
    }
}

// Function to handle smooth state transition
static void smooth_transition(uint32_t start, uint32_t end, uint32_t duration_ms) {
    int32_t step = (end - start) / NUM_STEPS;
    uint32_t pulse_width = start;
    uint32_t sleep_time = duration_ms / NUM_STEPS;

    for (uint32_t i = 0; i < NUM_STEPS; i++) {
        if (current_state != target_state) {
            // If state has changed, stop the transition and go to the new state
            break;
        }
        set_led_pulse(pulse_width);
        pulse_width += step;
        k_sleep(K_MSEC(sleep_time));
    }
}

// PWMLED thread entry point
static void pwmled_thread(void *unused1, void *unused2, void *unused3) {
    uint32_t blink_step = pwm_led0.period / 2;  // For fast blink in pairing state
    uint32_t breath_step = pwm_led0.period / NUM_STEPS;

    if (!pwm_is_ready_dt(&pwm_led0)) {
        printk("Error: PWM device %s is not ready\n", pwm_led0.dev->name);
        return;
    }

    while (1) {
        // Handle state transitions
        if (current_state != target_state) {
            // Complete any necessary transition logic
            switch (current_state) {
            case STATE_OFF:
                if (target_state == STATE_ADVERTISEMENT) {
                    smooth_transition(0, pwm_led0.period / 2, 1000); // Long fade in for advertisement
                }
                break;

            case STATE_PAIRING:
                if (target_state == STATE_OFF) {
                    smooth_transition(pwm_led0.period / 2, 0, 1000); // Fade out for pairing -> off
                }
                break;

            case STATE_ADVERTISEMENT:
                if (target_state == STATE_OFF) {
                    smooth_transition(pwm_led0.period / 2, 0, 1000); // Fade out for advertisement -> off
                }
                break;

            case STATE_CONNECTED:
                if (target_state == STATE_OFF) {
                    smooth_transition(pwm_led0.period / 2, 0, 1000); // Fade out for connected -> off
                }
                break;

            default:
                break;
            }

            current_state = target_state;
        }

        // Handle LED behavior based on state
        switch (current_state) {
        case STATE_OFF:
            set_led_pulse(0);  // LED is off
            k_sleep(K_MSEC(SLEEP_MSEC));
            break;

        case STATE_PAIRING: {
            // Fast blinking (no fading)
            for (int i = 0; i < 3; i++) {
                if (current_state != target_state) {
                    // If the state changes, exit the loop and apply the new state
                    break;
                }
                set_led_pulse(blink_step);
                k_sleep(K_MSEC(200));  // Blink on
                set_led_pulse(0);
                k_sleep(K_MSEC(200));  // Blink off
            }
            break;
        }

        case STATE_ADVERTISEMENT: {
            // Fast breathing, same rate as pairing blink
            for (int i = 0; i < NUM_STEPS; i++) {
                if (current_state != target_state) {
                    // If the state changes, exit the loop and apply the new state
                    break;
                }
                pulse_width = (i * pwm_led0.period) / NUM_STEPS;
                set_led_pulse(pulse_width);
                k_sleep(K_MSEC(200 / NUM_STEPS));  // Fast breathing rate
            }
            for (int i = NUM_STEPS; i > 0; i--) {
                if (current_state != target_state) {
                    // If the state changes, exit the loop and apply the new state
                    break;
                }
                pulse_width = (i * pwm_led0.period) / NUM_STEPS;
                set_led_pulse(pulse_width);
                k_sleep(K_MSEC(200 / NUM_STEPS));  // Fast breathing rate
            }
            break;
        }

        case STATE_CONNECTED: {
            // Slow breathing (not requested but could be added later)
            for (int i = 0; i < NUM_STEPS; i++) {
                if (current_state != target_state) {
                    // If the state changes, exit the loop and apply the new state
                    break;
                }
                pulse_width = (i * pwm_led0.period) / NUM_STEPS;
                set_led_pulse(pulse_width);
                k_sleep(K_MSEC(50));
            }
            for (int i = NUM_STEPS; i > 0; i--) {
                if (current_state != target_state) {
                    // If the state changes, exit the loop and apply the new state
                    break;
                }
                pulse_width = (i * pwm_led0.period) / NUM_STEPS;
                set_led_pulse(pulse_width);
                k_sleep(K_MSEC(50));
            }
            break;
        }

        default:
            break;
        }
    }
}

// Function to update LED state
void set_led_state(led_state_t state) {
    // Immediately change the target state, triggering the transition
    target_state = state;
}

int pwmled_init(void) {
    // Start the PWMLED thread
    k_thread_create(&pwmled_thread_data, pwmled_stack, K_THREAD_STACK_SIZEOF(pwmled_stack),
                    pwmled_thread, NULL, NULL, NULL,
                    PWMLED_THREAD_PRIORITY, 0, K_NO_WAIT);

    return 0;
}
