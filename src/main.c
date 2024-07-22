#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include "ble.h"
#include "gpio.h"
#include "hog.h"

LOG_MODULE_REGISTER(Revolute, LOG_LEVEL_DBG);

// Event handler for button events
static void button_event_handler(size_t idx, enum button_evt evt)
{
    int err;
    switch (idx)
    {
    case 0:
        if (evt == BUTTON_EVT_PRESSED)
        {

            struct hid_mouse_report_body report = {

                .buttons = 0b00000001,
                .d_x = 0x00,
                .d_y = 0x00,
                .d_wheel = 0x00,
            };

            mouse_bt_submit(&report);

            LOG_INF("Button 0 pressed");
        }
        else
        {
            LOG_INF("Button 0 released");

            struct hid_mouse_report_body report = {

                .buttons = 0b00000000,
                .d_x = 0x00,
                .d_y = 0x00,
                .d_wheel = 0x00,
            };

            mouse_bt_submit(&report);
        }
        break;
    case 1:
        if (evt == BUTTON_EVT_PRESSED)
        {
            err = gpio_pin_toggle_dt(&led);
            if (err < 0)
            {
                return;
            }

            struct hid_keyboard_report_body report = {

                .modifiers = 0b10000000, // | RGUI| RALT| RSHIFT| RCONTROL | LGUI| LALT| LSHIFT| LCONTROL|
                .keys = HID_KEY_Z,

            };
            keyboard_bt_submit(&report);

            LOG_INF("Button 1 pressed");
        }
        else
        {
            struct hid_keyboard_report_body report = {

                .modifiers = 0b00000000, // | RGUI| RALT| RSHIFT| RCONTROL | LGUI| LALT| LSHIFT| LCONTROL|
                .keys = 0,

            };
            keyboard_bt_submit(&report);

            LOG_INF("Button 1 released");
        }
        break;
    case 2:
        if (evt == BUTTON_EVT_PRESSED)
        {
            struct hid_consumer_report_body report = {
                .keys = HID_USAGE_CONSUMER_VOLUME_DECREMENT,
            };
            consumer_bt_submit(&report);

            LOG_INF("Button 2 pressed");
        }
        else
        {
            struct hid_consumer_report_body report = {
                .keys = 0,
            };
            consumer_bt_submit(&report);
            LOG_INF("Button 2 released");
        }
        break;
    case 3:
        if (evt == BUTTON_EVT_PRESSED)
        {
            deleteBond();
            LOG_INF("Button 3 pressed");
        }
        else
        {
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
    for (size_t i = 0; i < 4; i++)
    {
        button_init(i, button_event_handler);
    }

    k_work_init(&keyboard_bt_action_work, keyboard_bt_action_work_handler); // start button work queue
    k_work_init(&mouse_bt_action_work, mouse_bt_action_work_handler);       // start button work queue
    k_work_init(&consumer_bt_action_work, consumer_bt_action_work_handler); // start button work queue

    while (1)
    {
        int err = gpio_pin_toggle_dt(&led);
        if (err < 0)
        {
            return 0;
        }

        LOG_INF("a");

        k_msleep(1000);
    }

    return 0;
}
