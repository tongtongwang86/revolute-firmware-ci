#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>

#define NUM_STEPS   50U
#define SLEEP_MSEC  25U

#define PWM_LED0    DT_ALIAS(pwm_led0)
static const struct pwm_dt_spec pwm_led0 = PWM_DT_SPEC_GET(PWM_LED0);

#define PWMLED_STACK_SIZE 1024
#define PWMLED_THREAD_PRIORITY K_LOWEST_APPLICATION_THREAD_PRIO

static struct k_thread pwmled_thread_data;
static K_THREAD_STACK_DEFINE(pwmled_stack, PWMLED_STACK_SIZE);

// PWMLED thread entry point
static void pwmled_thread(void *unused1, void *unused2, void *unused3)
{
    uint32_t pulse_width = 0U;
    uint32_t step = pwm_led0.period / NUM_STEPS;
    uint8_t dir = 1U;
    int ret;

    printk("PWM-based LED fade\n");

    if (!pwm_is_ready_dt(&pwm_led0)) {
        printk("Error: PWM device %s is not ready\n", pwm_led0.dev->name);
        return;
    }

    while (1) {
        ret = pwm_set_pulse_dt(&pwm_led0, pulse_width);
        if (ret) {
            printk("Error %d: failed to set pulse width\n", ret);
            return;
        }
        printk("Using pulse width %d%%\n", 100 * pulse_width / pwm_led0.period);

        if (dir) {
            pulse_width += step;
            if (pulse_width >= pwm_led0.period) {
                pulse_width = pwm_led0.period - step;
                dir = 0U;
            }
        } else {
            if (pulse_width >= step) {
                pulse_width -= step;
            } else {
                pulse_width = step;
                dir = 1U;
            }
        }

        k_sleep(K_MSEC(SLEEP_MSEC));
    }
}

int pwmled_init(void)
{
    // Start the PWMLED thread
    k_thread_create(&pwmled_thread_data, pwmled_stack, K_THREAD_STACK_SIZEOF(pwmled_stack),
                    pwmled_thread, NULL, NULL, NULL,
                    PWMLED_THREAD_PRIORITY, 0, K_NO_WAIT);

    return 0;
}
