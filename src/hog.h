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

#define REPORT_ID_KEYBOARD 1
#define REPORT_ID_CONSUMER 2
#define REPORT_ID_MOUSE 3
#define REPORT_ID_CONTROLLER 4

#define HID_MAIN_VAL_DATA (0x00 << 0)
#define HID_MAIN_VAL_CONST (0x01 << 0)

#define HID_MAIN_VAL_ARRAY (0x00 << 1)
#define HID_MAIN_VAL_VAR (0x01 << 1)

#define HID_MAIN_VAL_ABS (0x00 << 2)
#define HID_MAIN_VAL_REL (0x01 << 2)

#define HID_MAIN_VAL_NO_WRAP (0x00 << 3)
#define HID_MAIN_VAL_WRAP (0x01 << 3)

#define HID_MAIN_VAL_LIN (0x00 << 4)
#define HID_MAIN_VAL_NON_LIN (0x01 << 4)

#define HID_MAIN_VAL_PREFERRED (0x00 << 5)
#define HID_MAIN_VAL_NO_PREFERRED (0x01 << 5)

#define HID_MAIN_VAL_NO_NULL (0x00 << 6)
#define HID_MAIN_VAL_NULL (0x01 << 6)

#define HID_MAIN_VAL_NON_VOL (0x00 << 7)
#define HID_MAIN_VAL_VOL (0x01 << 7)

#define HID_MAIN_VAL_BIT_FIELD (0x00 << 8)
#define HID_MAIN_VAL_BUFFERED_BYTES (0x01 << 8)

#define HID_REPORT_ID_KEYBOARD 0x01
#define HID_REPORT_ID_LEDS 0x01
#define HID_REPORT_ID_CONSUMER 0x02
#define HID_REPORT_ID_MOUSE 0x03
#define HID_REPORT_ID_CONTROLLER 0x04

#define INPUT_REPORT_SIZE 8

struct hids_report
{
    uint8_t id;   /* report id */
    uint8_t type; /* report type */
} __packed;

enum {
    HIDS_INPUT = 0x01,
    HIDS_OUTPUT = 0x02,
    HIDS_FEATURE = 0x03,
};

enum {
    HIDS_REMOTE_WAKE = BIT(0),
    HIDS_NORMALLY_CONNECTABLE = BIT(1),
};

#endif // HOG_H