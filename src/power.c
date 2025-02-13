#include "power.h"
#include <zephyr/kernel.h>

#define THREAD_STACK_SIZE 1024
#define THREAD_PRIORITY 7
#define THREAD_SLEEP_TIME_MS 10000

led_state_t target_state = STATE_ADVERTISEMENT;

LOG_MODULE_REGISTER(power, LOG_LEVEL_INF);

#define MOSFET_NODE DT_ALIAS(mosfet)
static const struct gpio_dt_spec mosfet = GPIO_DT_SPEC_GET(MOSFET_NODE, gpios);

static struct k_thread power_thread_data;
#define POWER_STACK_SIZE 1024
static K_THREAD_STACK_DEFINE(power_stack, POWER_STACK_SIZE);
#define POWER_THREAD_PRIORITY K_LOWEST_APPLICATION_THREAD_PRIO

#define SW3_NODE DT_ALIAS(sw3) // button to attach the interrupt to
static const struct gpio_dt_spec sw3_button = GPIO_DT_SPEC_GET(SW3_NODE, gpios);


void power_off(void) {

    
    gpio_pin_set_dt(&mosfet, 1); // disconnect sensor from power
    disable_bluetooth();
    button_uninit();
    int rc;

    // Configure SW3 GPIO as input with interrupt
    rc = gpio_pin_configure_dt(&sw3_button, GPIO_INPUT);
    if (rc < 0) {
        printk("Failed to configure SW3 GPIO (%d)\n", rc);
        return;
    }

        // Configure SW3 GPIO interrupt (trigger on level active)
    rc = gpio_pin_interrupt_configure_dt(&sw3_button, GPIO_INT_LEVEL_ACTIVE);
    if (rc < 0) {
        printk("Failed to configure interrupt for SW3 GPIO (%d)\n", rc);
        return;
    }
    

    printk("shuttingoff\n");
   sys_poweroff();
    
}

void power_standby(void){


    // suspend_magnetic_thread();

    const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));

    // Suspend I2C
    if (device_is_ready(i2c_dev)) {
        pm_device_action_run(i2c_dev, PM_DEVICE_ACTION_SUSPEND);
        printk("I2C suspended.\n");
    }

    gpio_pin_set_dt(&mosfet, 1); // turn mosfet off
    LOG_INF("standby\n");

    
}

void power_resume(void){

    gpio_pin_set_dt(&mosfet, 0); // turn mosfet back on
    const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));

    // Resume I2C
    if (device_is_ready(i2c_dev)) {
        int ret = pm_device_action_run(i2c_dev, PM_DEVICE_ACTION_RESUME);
        if (ret == 0) {
            printk("I2C resumed.\n");
        } else {
            printk("Failed to resume I2C: %d\n", ret);
        }
    } else {
        printk("I2C device is not ready.\n");
    }
    // resume_magnetic_thread();
    LOG_INF("resumed\n");

}



