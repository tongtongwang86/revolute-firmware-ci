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
#define REV_GYRO_UUID         BT_UUID_DECLARE_128(0x00001530, 0x1212, 0xefde, 0x1523, 0x785feabcd133)





int rev_send_gyro(float r, float i, float j, float k);




#endif // REVSVC_H
