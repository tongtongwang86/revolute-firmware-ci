#ifndef HOG_H
#define HOG_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <hid_usage.h>
#include <hid_usage_pages.h>
#include <zephyr/usb/class/usb_hid.h>
#include <bluetooth/services/hids.h>

//hog_svc.attrs[i]
    // 5 for keyboard
    // 9 for consumer
    // 13 for mouse

#define MOUSE_MSGQ_ARRAY_SIZE 32
#define KEYBOARD_MSGQ_ARRAY_SIZE 32
#define CONSUMER_MSGQ_ARRAY_SIZE 32

struct hids_report
{
    uint8_t id;   /* report id */
    uint8_t type; /* report type */
} __packed;

struct hid_keyboard_report_body {
    uint8_t modifiers;
    uint8_t keys;
} __packed;

struct hid_consumer_report_body {
    uint8_t keys;
} __packed;

struct hid_mouse_report_body {
    int8_t buttons;
    int8_t d_x;
    int8_t d_y;
    int8_t d_wheel;
} __packed;

#define REPORT_ID_KEYBOARD 1
#define REPORT_ID_CONSUMER 2
#define REPORT_ID_MOUSE 3

#define ZMK_HID_MAIN_VAL_DATA (0x00 << 0)
#define ZMK_HID_MAIN_VAL_CONST (0x01 << 0)

#define ZMK_HID_MAIN_VAL_ARRAY (0x00 << 1)
#define ZMK_HID_MAIN_VAL_VAR (0x01 << 1)

#define ZMK_HID_MAIN_VAL_ABS (0x00 << 2)
#define ZMK_HID_MAIN_VAL_REL (0x01 << 2)

#define ZMK_HID_MAIN_VAL_NO_WRAP (0x00 << 3)
#define ZMK_HID_MAIN_VAL_WRAP (0x01 << 3)

#define ZMK_HID_MAIN_VAL_LIN (0x00 << 4)
#define ZMK_HID_MAIN_VAL_NON_LIN (0x01 << 4)

#define ZMK_HID_MAIN_VAL_PREFERRED (0x00 << 5)
#define ZMK_HID_MAIN_VAL_NO_PREFERRED (0x01 << 5)

#define ZMK_HID_MAIN_VAL_NO_NULL (0x00 << 6)
#define ZMK_HID_MAIN_VAL_NULL (0x01 << 6)

#define ZMK_HID_MAIN_VAL_NON_VOL (0x00 << 7)
#define ZMK_HID_MAIN_VAL_VOL (0x01 << 7)

#define ZMK_HID_MAIN_VAL_BIT_FIELD (0x00 << 8)
#define ZMK_HID_MAIN_VAL_BUFFERED_BYTES (0x01 << 8)

#define ZMK_HID_REPORT_ID_KEYBOARD 0x01
#define ZMK_HID_REPORT_ID_LEDS 0x01
#define ZMK_HID_REPORT_ID_CONSUMER 0x02
#define ZMK_HID_REPORT_ID_MOUSE 0x03

#define INPUT_REPORT_SIZE 8

enum {
    HIDS_INPUT = 0x01,
    HIDS_OUTPUT = 0x02,
    HIDS_FEATURE = 0x03,
};

enum {
    HIDS_REMOTE_WAKE = BIT(0),
    HIDS_NORMALLY_CONNECTABLE = BIT(1),
};

extern struct k_work button_action_work;
extern struct k_work keyboard_bt_action_work;
extern struct k_work mouse_bt_action_work;
extern struct k_work consumer_bt_action_work;

extern struct bt_hids_info info;
extern uint8_t simulate_input;
extern uint8_t ctrl_point;

void button_action_work_handler(struct k_work *work);

void keyboard_bt_action_work_handler(struct k_work *work);
void mouse_bt_action_work_handler(struct k_work *work);
void consumer_bt_action_work_handler(struct k_work *work);



#endif // HOG_H
