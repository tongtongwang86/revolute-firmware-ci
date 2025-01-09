#ifndef REVSVC_H
#define REVSVC_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/settings/settings.h>



#define REV_SVC_UUID           BT_UUID_DECLARE_128(0x00001523, 0x1212, 0xefde, 0x1523, 0x785feabcd133)
// read int
#define REV_STATS_UUID         BT_UUID_DECLARE_128(0x00001524, 0x1212, 0xefde, 0x1523, 0x785feabcd133)

#define REV_READCONFIG_UUID    BT_UUID_DECLARE_128(0x00001525, 0x1212, 0xefde, 0x1523, 0x785feabcd133)

#define REV_WRITECONFIG_UUID   BT_UUID_DECLARE_128(0x00001526, 0x1212, 0xefde, 0x1523, 0x785feabcd133)


struct config_profile {
    uint8_t deadzone;
    uint8_t up_report[8];
    uint8_t up_identPerRev;
    uint8_t up_transport;
    uint8_t dn_report[8];
    uint8_t dn_identPerRev;
    uint8_t dn_transport;
};

extern struct config_profile config;

// int rev_send_gyro(float r, float i, float j, float k);

// extern bool notify_mysensor_enabled;

#endif // REVSVC_H