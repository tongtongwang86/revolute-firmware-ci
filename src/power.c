#include "power.h"
#include <zephyr/kernel.h>

led_state_t target_state = STATE_ADVERTISEMENT;

LOG_MODULE_REGISTER(power, LOG_LEVEL_INF);

#define MOSFET_NODE DT_ALIAS(mosfet)
static const struct gpio_dt_spec mosfet = GPIO_DT_SPEC_GET(MOSFET_NODE, gpios);


#define SW3_NODE DT_ALIAS(sw3) // button to attach the interrupt to
static const struct gpio_dt_spec sw3_button = GPIO_DT_SPEC_GET(SW3_NODE, gpios);


void power_off(void) {
    const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));

 

    // disable Bluetooth
    suspend_battery_update();
    disable_bluetooth();
    suspend_magnetic_thread();

       // Suspend I2C
    if (device_is_ready(i2c_dev)) {
        pm_device_action_run(i2c_dev, PM_DEVICE_ACTION_SUSPEND);
        printk("I2C suspended.\n");
    }
    
    gpio_pin_set_dt(&mosfet, 1); // Turns the LED on
    

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


    printk("Entering deep sleep mode...\n");
   sys_poweroff();
    
}

void power_on(void) {

    gpio_pin_set_dt(&mosfet, 0); // Turns the LED on
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
    resume_magnetic_thread();
   bluetooth_init();
   resume_battery_update();
   

LOG_INF("power on\n");

}


