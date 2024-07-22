#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include "ble.h"
#include "gpio.h"

LOG_MODULE_REGISTER(Revolute, LOG_LEVEL_DBG);

// Event handler for button events
static void button_event_handler(size_t idx, enum button_evt evt)
{
    int err;
    switch (idx)
    {
    case 0:
        if (evt == BUTTON_EVT_PRESSED) {
            LOG_INF("Button 0 pressed");
        } else {
            LOG_INF("Button 0 released");
        }
        break;
    case 1:
        if (evt == BUTTON_EVT_PRESSED) {
            err = gpio_pin_toggle_dt(&led);
            if (err < 0) {
                return;
            }
            LOG_INF("Button 1 pressed");
        } else {
            LOG_INF("Button 1 released");
        }
        break;
    case 2:
        if (evt == BUTTON_EVT_PRESSED) {
            LOG_INF("Button 2 pressed");
        } else {
            err = gpio_pin_toggle_dt(&led);
            if (err < 0) {
                return;
            }
            LOG_INF("Button 2 released");
        }
        break;
    case 3:
        if (evt == BUTTON_EVT_PRESSED) {
            deleteBond();
            LOG_INF("Button 3 pressed");
        } else {
            LOG_INF("Button 3 released");
        }
        break;
    default:
        LOG_ERR("Unknown button %zu event", idx);
        break;
    }
}

int main(void)
{
    // Initialize LEDs
    ledInit();

    // Initialize BLE
    enableBle();
    startAdv();

    // Initialize buttons
    for (size_t i = 0; i < 4; i++) {
        button_init(i, button_event_handler);
    }

    while (1)
    {
        int err = gpio_pin_toggle_dt(&led);
        if (err < 0) {
            return 0;
        }

        LOG_INF("a");

        k_msleep(1000);
    }

    return 0;
}
