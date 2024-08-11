#ifndef REVSVC_H
#define REVSVC_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/class/usb_hid.h>



#define REV_SVC_UUID          BT_UUID_DECLARE_128(0x00001523, 0x1212, 0xefde, 0x1523, 0x785feabcd133)
// read int
#define REV_ANGLE_UUID        BT_UUID_DECLARE_128(0x00001524, 0x1212, 0xefde, 0x1523, 0x785feabcd133)

#define REV_READCONFIG_UUID        BT_UUID_DECLARE_128(0x00001522, 0x1212, 0xefde, 0x1523, 0x785feabcd133)
// send uint8 in hex
#define REV_MODE_UUID         BT_UUID_DECLARE_128(0x00001525, 0x1212, 0xefde, 0x1523, 0x785feabcd133)
// send uint8 in hex
#define REV_RESOLUTION_UUID   BT_UUID_DECLARE_128(0x00001526, 0x1212, 0xefde, 0x1523, 0x785feabcd133)
// send uint8 in hex
#define REV_DEADZONE_UUID     BT_UUID_DECLARE_128(0x00001527, 0x1212, 0xefde, 0x1523, 0x785feabcd133)
// send uint8 in hex

#define REV_UPREPORT_UUID     BT_UUID_DECLARE_128(0x00001528, 0x1212, 0xefde, 0x1523, 0x785feabcd133)
// example report 000000000000001D key 29 HID_KEY_Z is 1D
#define REV_DOWNREPORT_UUID   BT_UUID_DECLARE_128(0x00001529, 0x1212, 0xefde, 0x1523, 0x785feabcd133)
// example report 000000000000001C key29 HID_KEY_Y is 1C

struct config_profile {
    int mode;
    int identPerRev;
    int deadzone; //deadzone in degrees
    uint8_t upReport[8];
    uint8_t downReport[8];
};

extern struct config_profile config;

extern uint16_t angle_value;

ssize_t read_callback(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
ssize_t write_callback(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags);


void save_config(void);
void load_config(void);



#endif // REVSVC_H
