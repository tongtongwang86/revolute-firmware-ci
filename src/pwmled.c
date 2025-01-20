
#include "pwmled.h"

LOG_MODULE_REGISTER(pwmled, LOG_LEVEL_INF);
#define NUM_STEPS      100U  // Number of steps for both fade-in and fade-out
#define FADE_DURATION_MS 1000U // Duration of fade-in and fade-out in milliseconds
#define SLEEP_MSEC     (FADE_DURATION_MS / NUM_STEPS)  // Adjust the sleep for the fade duration

#define PWM_LED0       DT_ALIAS(pwm_led0)
static const struct pwm_dt_spec pwm_led0 = PWM_DT_SPEC_GET(PWM_LED0);

#define PWMLED_STACK_SIZE 1024
#define PWMLED_THREAD_PRIORITY K_LOWEST_APPLICATION_THREAD_PRIO

static struct k_thread pwmled_thread_data;
static K_THREAD_STACK_DEFINE(pwmled_stack, PWMLED_STACK_SIZE);

// Define current and target LED state
static led_state_t current_state = STATE_ADVERTISEMENT;

static uint32_t pulse_width = 0;  // Track the current LED brightness

// Function to set LED pulse width
static void set_led_pulse(uint32_t pulse_width) {
    int ret = pwm_set_pulse_dt(&pwm_led0, pulse_width);
    if (ret) {
        printk("Error %d: failed to set pulse width\n", ret);
    }
}


void suspend_pwmled(void) {
   
    k_thread_suspend(&pwmled_thread_data);
    LOG_INF("Battery update thread suspended");
}

// Function for fade-in effect (0 -> max brightness -> 0)
static void fade_in(void) {
    uint32_t step_up = pwm_led0.period / NUM_STEPS;  // Fade-up step
    uint32_t step_down = step_up;                   // Fade-down step (same size for symmetry)
    
    // Fade up: 0 -> max brightness
    for (uint32_t i = 0; i < NUM_STEPS; i++) {
        pulse_width = i * step_up;
        set_led_pulse(pulse_width);  // Inverted
        k_sleep(K_MSEC(SLEEP_MSEC));
    }

    // Fade down: max brightness -> 0
    for (uint32_t i = NUM_STEPS; i > 0; i--) {
        pulse_width = i * step_down;
        set_led_pulse(pulse_width);  // Inverted
        k_sleep(K_MSEC(SLEEP_MSEC));
    }
    pulse_width = 0;
    set_led_pulse(pulse_width);  // Ensure the LED is completely off
}


// Function for fade-out effect (from current brightness -> 0)
static void fade_out(void) {
    uint32_t current_brightness = pulse_width;
    uint32_t step_down = current_brightness / NUM_STEPS;

    // Fade down: current brightness -> 0
    for (uint32_t i = 0; i < NUM_STEPS; i++) {
        pulse_width = current_brightness - (i * step_down);
        set_led_pulse(pulse_width);  // Inverted
        k_sleep(K_MSEC(SLEEP_MSEC));
    }

    pulse_width = 0;
    set_led_pulse(pulse_width);  // Ensure the LED is completely off
}


// PWMLED thread entry point
static void pwmled_thread(void *unused1, void *unused2, void *unused3) {
    uint32_t blink_step = pwm_led0.period / 2;  // For fast blink in pairing state
    uint32_t breath_step = pwm_led0.period / NUM_STEPS;

    fade_in();

    while (1) {
        // Handle state transitions
        if (current_state != target_state) {
             if (target_state == STATE_OFF) {
                // Fade-out from current brightness to 0 when turning from any state to off
                fade_out();
            }

            current_state = target_state;  // Apply the new state immediately
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
                k_sleep(K_MSEC(2000 / NUM_STEPS));
            }
            for (int i = NUM_STEPS; i > 0; i--) {
                if (current_state != target_state) {
                    // If the state changes, exit the loop and apply the new state
                    break;
                }
                pulse_width = (i * pwm_led0.period) / NUM_STEPS;
                set_led_pulse(pulse_width);
                k_sleep(K_MSEC(2000 / NUM_STEPS));
            }
            break;
        }

        default:
            break;
        }
    }
}



int pwmled_init(void) {
    // Ensure the LED starts off
    current_state = STATE_ADVERTISEMENT;
    target_state = STATE_ADVERTISEMENT;
   
     if (!pwm_is_ready_dt(&pwm_led0)) {
        printk("Error: PWM device %s is not ready\n", pwm_led0.dev->name);
        return -1; // Return error if PWM device isn't ready
    }

    // Set the LED pulse width to 0 immediately (turn off LED before the thread runs)
    set_led_pulse(0);

    // Start the PWMLED thread
    k_thread_create(&pwmled_thread_data, pwmled_stack, K_THREAD_STACK_SIZEOF(pwmled_stack),
                    pwmled_thread, NULL, NULL, NULL,
                    PWMLED_THREAD_PRIORITY, 0, K_NO_WAIT);

    return 0;
}