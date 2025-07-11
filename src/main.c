#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include "sensor.h"

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

void on_cw_rotation(void) {
    gpio_pin_toggle_dt(&led);
    // printk("CW Detected!\n");
}

void on_ccw_rotation(void) {
    gpio_pin_toggle_dt(&led);
    // printk("CCW Detected!\n");
}

void main(void) {
    if (!gpio_is_ready_dt(&led)) return;
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

    register_cw_callback(on_cw_rotation);
    register_ccw_callback(on_ccw_rotation);

    sensor_init();

    while (1) {
        sensor_read();

    }
}
