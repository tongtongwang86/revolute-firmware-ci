#include "power.h"
#include <zephyr/kernel.h>

#define THREAD_STACK_SIZE 1024
#define THREAD_PRIORITY 7
#define THREAD_SLEEP_TIME_MS 10000


LOG_MODULE_REGISTER(power, LOG_LEVEL_INF);


#define POWER_STACK_SIZE 1024
#define POWER_THREAD_PRIORITY K_LOWEST_APPLICATION_THREAD_PRIO

#define SW3_NODE DT_ALIAS(sw0) // button to attach the interrupt to
static const struct gpio_dt_spec sw3_button = GPIO_DT_SPEC_GET(SW3_NODE, gpios);


void power_off(void) {

    

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



