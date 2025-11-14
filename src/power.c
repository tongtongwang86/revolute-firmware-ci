
#include <zephyr/kernel.h>
#include "power.h"
#include "sensor.h"
#include "batterylvl.h"
#include "charger.h"
#include <zephyr/sys/poweroff.h>

LOG_MODULE_REGISTER(power, LOG_LEVEL_INF);

#define SW3_NODE DT_ALIAS(sw0) // button to attach the interrupt to
static const struct gpio_dt_spec sw3_button = GPIO_DT_SPEC_GET(SW3_NODE, gpios);

static struct k_work_delayable power_off_work;


static void power_off_handler(struct k_work *work);

// Static delayed work structure
static struct k_work_delayable power_off_work;

static int power_off_work_init(const struct device *dev)
{
    ARG_UNUSED(dev);
    k_work_init_delayable(&power_off_work, power_off_handler);
    return 0;
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
    /* Stop periodic activity (charger, battery updates, sensor polling)
     * to reduce I2C/charger activity before power off. */
    // charger_stop();
    // sensor_stop();
    // battery_stop();

    /* Allow some time for the drivers to finish transactions */
    k_msleep(50);

    /* Put the TLV493 into poweroff mode */
    configure_tlv493_poweroff_mode();

    LOG_INF("Preparing to power off");
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