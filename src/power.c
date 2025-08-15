
#include <zephyr/kernel.h>
#include "power.h"
#include "sensor.h"

LOG_MODULE_REGISTER(power, LOG_LEVEL_INF);

#define SW3_NODE DT_ALIAS(sw0) // button to attach the interrupt to
static const struct gpio_dt_spec sw3_button = GPIO_DT_SPEC_GET(SW3_NODE, gpios);

static struct k_work_delayable power_off_work;


static void power_off_handler(struct k_work *work);

// Static delayed work structure
static struct k_work_delayable power_off_work;

void power_off_work_init(void)
{
    k_work_init_delayable(&power_off_work, power_off_handler);
}

static void power_off_handler(struct k_work *work)
{
    power_off();
}


void schedule_power_off(int delay_ms)
{
    LOG_INF("Scheduling power off in %d ms", delay_ms);
    k_work_schedule(&power_off_work, K_MSEC(delay_ms));
}


void power_off(void) {
    // tlv493_general_reset();
    // Allow time for sensor reset
    // k_msleep(5);
    // configure_tlv493_poweroff_mode();
    //sleep 
    configure_tlv493_poweroff_mode();


    LOG_INF("aaa");
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
    
    k_msleep(2000);

    printk("shuttingoff\n");
   sys_poweroff();
    
}


SYS_INIT(power_off_work_init, APPLICATION, 50);