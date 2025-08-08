#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include "haptic.h"

#define LED4_NODE DT_ALIAS(hap0)

#if !DT_NODE_HAS_STATUS(LED4_NODE, okay)
#error "Unsupported board: led4 devicetree alias is not defined"
#endif

static const struct gpio_dt_spec led4 = GPIO_DT_SPEC_GET(LED4_NODE, gpios);
static struct k_work led4_pulse_work;

static void led4_pulse_handler(struct k_work *work)
{
    gpio_pin_set_dt(&led4, 1);   // LED ON
    k_msleep(10);                // Pulse duration
    gpio_pin_set_dt(&led4, 0);   // LED OFF
}

void led4_pulse_submit(void)
{
    static bool initialized = false;

    if (!initialized) {
        if (!device_is_ready(led4.port)) {
            printk("LED4 device not ready\n");
            return;
        }

        int ret = gpio_pin_configure_dt(&led4, GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            printk("Failed to configure LED4 pin\n");
            return;
        }

        k_work_init(&led4_pulse_work, led4_pulse_handler);
        initialized = true;
    }

    k_work_submit(&led4_pulse_work);
}
