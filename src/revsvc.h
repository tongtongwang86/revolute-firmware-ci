#ifndef REVSVC_H
#define REVSVC_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

#define REV_SVC_UUID  BT_UUID_DECLARE_128(0x00001523, 0x1212, 0xefde, 0x1523, 0x785feabcd133)
#define REV_READ_UUID BT_UUID_DECLARE_128(0x00001524, 0x1212, 0xefde, 0x1523, 0x785feabcd133)
#define REV_WRITE_UUID BT_UUID_DECLARE_128(0x00001525, 0x1212, 0xefde, 0x1523, 0x785feabcd133)

struct config_profile {
    int mode;
    int identPerRev;
    int deadzone; //deadzone in degrees
    uint8_t upReport[8];
    uint8_t downReport[8];
};

extern struct config_profile config;

extern uint16_t read_value;

ssize_t read_callback(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
ssize_t write_callback(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags);

void spinUpdateThread(void);

#endif // REVSVC_H
