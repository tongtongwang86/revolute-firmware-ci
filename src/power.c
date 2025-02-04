#include "power.h"
#include <zephyr/kernel.h>

led_state_t target_state = STATE_ADVERTISEMENT;

LOG_MODULE_REGISTER(power, LOG_LEVEL_INF);

#define MOSFET_NODE DT_ALIAS(mosfet)
static const struct gpio_dt_spec mosfet = GPIO_DT_SPEC_GET(MOSFET_NODE, gpios);


#define SW3_NODE DT_ALIAS(sw3) // button to attach the interrupt to
static const struct gpio_dt_spec sw3_button = GPIO_DT_SPEC_GET(SW3_NODE, gpios);


void power_off(void) {

    
    gpio_pin_set_dt(&mosfet, 1); // disconnect sensor from power
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
    printk("Entering deep sleep mode...\n");
   sys_poweroff();
    
}




