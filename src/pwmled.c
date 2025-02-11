#include "pwmled.h"
#include <math.h>

LOG_MODULE_REGISTER(pwmled, LOG_LEVEL_INF);

#define PWM_LED0 DT_ALIAS(pwm_led0)
static const struct pwm_dt_spec pwm_led0 = PWM_DT_SPEC_GET(PWM_LED0);

#define PWMLED_STACK_SIZE 1024
#define PWMLED_THREAD_PRIORITY K_LOWEST_APPLICATION_THREAD_PRIO
#define M_PI 3.141592

static struct k_thread pwmled_thread_data;
static K_THREAD_STACK_DEFINE(pwmled_stack, PWMLED_STACK_SIZE);

extern enum power_type power_status;
extern enum advertising_type advertising_status;

static float brightness = 0;      // Current LED brightness (0 to 1)
static float velocity = 0;        // Rate of change of brightness
static float target_brightness = 0; // Target LED brightness
static float mass = 5, spring_k = 30, damping_b = 4;  // Physics properties

static void set_led_pulse(float normalized_brightness) {
    uint32_t pulse_width = (uint32_t)(normalized_brightness * pwm_led0.period);
    int ret = pwm_set_pulse_dt(&pwm_led0, pulse_width);
    if (ret) {
        printk("Error %d: failed to set pulse width\n", ret);
    }
}

static void update_physics(float dt) {
    // Calculate acceleration from Hooke's Law and damping
    float acceleration = (-spring_k * (brightness - target_brightness) - damping_b * velocity) / mass;
    
    // Integrate velocity and position using simple Euler integration
    velocity += acceleration * dt;
    brightness += velocity * dt;

    // Clamp brightness between 0 and 1

}

static void fade_in(void) {
    uint32_t start_time = k_uptime_get();
    while (k_uptime_get() - start_time < 2000) { // Run for 2 seconds
        float time_elapsed = (k_uptime_get() - start_time) / 1000.0f; // Convert to seconds
        target_brightness = 0.5f * (1 - cosf(M_PI * time_elapsed)); // Smooth transition

        set_led_pulse(target_brightness);
        k_sleep(K_MSEC(10)); // Sleep for 10ms
    }
}

static void pwmled_thread(void *unused1, void *unused2, void *unused3) {
    fade_in();

    while (1) {
        // Update physics parameters based on state
        if (power_status == PWR_OFF) {
            mass = 10;
            spring_k = 50;
            damping_b = 4;
            target_brightness = 0;
        } else if (power_status == PWR_HOLD) {
            mass = 5;
            spring_k = 30;
            damping_b = 4;
            target_brightness = 0.5;
        } else if (power_status == PWR_ON) {
            switch (advertising_status) {
                case ADV_NONE:
                    mass = 1;
                    spring_k = 10;
                    damping_b = 2;
                    target_brightness = 0.5 + 0.33 * sin(k_uptime_get() * 0.002);  // Slow breathing
                    break;
                case ADV_FILTER:
                    mass = .4;
                    spring_k = 40;
                    damping_b = 5;
                    target_brightness = 0.5 + 0.6 * sin(k_uptime_get() * 0.01); // Fast breathing
                    break;
                case ADV_CONN:
                    mass = 1;
                    spring_k = 40;
                    damping_b = 3;
                    target_brightness =  0.5 + 5 * sin(k_uptime_get() * 0.01); // Fast breathing
                    break;
            }
        }

        // Run the physics simulation step (assuming 10ms per cycle)
        update_physics(0.01);
        

        float normalized_brightness;
        LOG_INF("brightness: %f", brightness);
        if (brightness < 0) {
            normalized_brightness = 0;
            velocity = 0;  // Stop movement when hitting the lower bound
        }else if (brightness > 1) {
            normalized_brightness = 1;
            velocity = 0;  // Stop movement when hitting the upper bound
        } else {
            normalized_brightness = brightness;
        }

        // Apply LED brightness
        set_led_pulse(normalized_brightness);


        // Sleep for 10ms
        k_sleep(K_MSEC(10));
    }
}

int pwmled_init(void) {
    if (!pwm_is_ready_dt(&pwm_led0)) {
        printk("Error: PWM device %s is not ready\n", pwm_led0.dev->name);
        return -1;
    }

    set_led_pulse(0);
    

    k_thread_create(&pwmled_thread_data, pwmled_stack, K_THREAD_STACK_SIZEOF(pwmled_stack),
                    pwmled_thread, NULL, NULL, NULL,
                    PWMLED_THREAD_PRIORITY, 0, K_NO_WAIT);

    return 0;
}

SYS_INIT(pwmled_init, APPLICATION, 50);
