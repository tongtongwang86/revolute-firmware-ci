#include "pwmled.h"
#include "statemanager.h"
#include <math.h>

LOG_MODULE_REGISTER(pwmled, LOG_LEVEL_INF);

#define PWM_LED0 DT_ALIAS(pwm_led0)
static const struct pwm_dt_spec pwm_led0 = PWM_DT_SPEC_GET(PWM_LED0);

#define PWMLED_STACK_SIZE 1024
// #define PWMLED_THREAD_PRIORITY K_LOWEST_APPLICATION_THREAD_PRIO
#define PWMLED_THREAD_PRIORITY 6
#define M_PI 3.141592

static struct k_thread pwmled_thread_data;
static K_THREAD_STACK_DEFINE(pwmled_stack, PWMLED_STACK_SIZE);
static bool pwmled_thread_started;

/* Idle tick when the animation has settled on a constant target. A breathing
 * pattern needs the fast tick; a steady level does not, and the PWM peripheral
 * plus a 20 Hz wakeup is not free. */
#define PWMLED_TICK_MS      50
#define PWMLED_IDLE_TICK_MS 250
#define PWMLED_SETTLED_EPS  0.005f

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
    const float step = 0.05f; // Adjust step size for desired speed
    const float max_brightness = 1.0f;
    
    // Fade in
    for (float brightness = 0.0f; brightness <= max_brightness; brightness += step) {
        set_led_pulse(brightness);
        k_sleep(K_MSEC(40)); // Sleep for 10ms
    }

    // Fade out
    for (float brightness = max_brightness; brightness >= 0.0f; brightness -= step) {
        set_led_pulse(brightness);
        k_sleep(K_MSEC(65)); // Sleep for 10ms
    }
}




static void pwmled_thread(void *unused1, void *unused2, void *unused3) {
    fade_in();

    while (1) {
        bool breathing = false;

        /* The LED reports the run state first and the link state second, so a
         * device that is asleep or holding looks different from one that is
         * hunting for a host. */
        if (power_status == PWR_OFF || isOff) {
            mass = 1;
            spring_k = 3;
            damping_b = 7;
            target_brightness = -5;
        } else if (power_status == PWR_HOLD || onhold) {
            mass = 1;
            spring_k = 50;
            damping_b = 4;
            target_brightness = 0.2f;
        } else {
            switch (advertising_status) {
            case ADV_NONE:
                if (power_status == PWR_STANDBY) {
                    mass = 1;
                    spring_k = 50;
                    damping_b = 4;
                    target_brightness = 0.1f;
                } else {
                    mass = 1;
                    spring_k = 10;
                    damping_b = 2;
                    /* Slow breathing: connected and awake. */
                    target_brightness = 0.5f + 0.33f * sinf(k_uptime_get() * 0.002f);
                    breathing = true;
                }
                break;
            case ADV_FILTER:
                mass = .4;
                spring_k = 40;
                damping_b = 5;
                /* Fast breathing: advertising to a known host. */
                target_brightness = 0.5f + 0.6f * sinf(k_uptime_get() * 0.01f);
                breathing = true;
                break;
            case ADV_CONN:
                mass = 1;
                spring_k = 40;
                damping_b = 3;
                /* Fast, hard breathing: open for pairing. */
                target_brightness = 0.5f + 5.0f * sinf(k_uptime_get() * 0.01f);
                breathing = true;
                break;
            }
        }

        update_physics(0.05f);

        float normalized_brightness;

        if (brightness < 0) {
            normalized_brightness = 0;
            velocity = 0;  // Stop movement when hitting the lower bound
        } else if (brightness > 1) {
            normalized_brightness = 1;
            velocity = 0;  // Stop movement when hitting the upper bound
        } else {
            normalized_brightness = brightness;
        }

        set_led_pulse(normalized_brightness);

        bool settled = !breathing &&
                       (fabsf(velocity) < PWMLED_SETTLED_EPS) &&
                       (fabsf(brightness - target_brightness) < PWMLED_SETTLED_EPS);

        k_sleep(K_MSEC(settled ? PWMLED_IDLE_TICK_MS : PWMLED_TICK_MS));
    }
}

void pwmled_shutdown(void) {
    /* Stop the animation thread before touching the duty cycle. Otherwise the
     * thread and this call race for the PWM, and whichever writes last is the
     * value the pin latches into System OFF. */
    if (pwmled_thread_started) {
        k_thread_abort(&pwmled_thread_data);
        pwmled_thread_started = false;
    }

    /* Ramp down from wherever the animation actually was. */
    for (float level = brightness; level > 0.0f; level -= 0.05f) {
        set_led_pulse(level);
        k_sleep(K_MSEC(20));
    }

    brightness = 0.0f;
    velocity = 0.0f;
    target_brightness = 0.0f;
    set_led_pulse(0.0f);

    /* Hand the pin back to pinctrl's low-power state so a retained GPIO level
     * cannot keep the LED lit through System OFF. */
    int ret = pm_device_action_run(pwm_led0.dev, PM_DEVICE_ACTION_SUSPEND);

    if (ret && ret != -EALREADY && ret != -ENOTSUP) {
        printk("Failed to suspend PWM (err %d)\n", ret);
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
    pwmled_thread_started = true;

    return 0;
}

SYS_INIT(pwmled_init, APPLICATION, 50);
