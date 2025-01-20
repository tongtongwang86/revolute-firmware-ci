#ifndef REVSVC_H
#define REVSVC_H

#include <zephyr/kernel.h>
#include <stdio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/settings/settings.h>

// thread stuff
#define REV_SVC_THREAD_STACK_SIZE 1024  // Adjust based on requirements
#define REV_SVC_THREAD_PRIORITY 5  // Priority for rev_svc_loop thread
static struct k_thread rev_svc_thread_data;
static K_THREAD_STACK_DEFINE(rev_svc_stack, REV_SVC_THREAD_STACK_SIZE); 


extern struct k_sem stats_notification_sem;

#define REV_SVC_UUID           BT_UUID_DECLARE_128(0x00001523, 0x1212, 0xefde, 0x1523, 0x785feabcd133)
// read int
#define REV_STATS_UUID         BT_UUID_DECLARE_128(0x00001524, 0x1212, 0xefde, 0x1523, 0x785feabcd133)
// Define the UUID for the read characteristic
#define REV_READCONFIG_UUID    BT_UUID_DECLARE_128(0x00001525, 0x1212, 0xefde, 0x1523, 0x785feabcd133)

#define REV_WRITECONFIG_UUID   BT_UUID_DECLARE_128(0x00001526, 0x1212, 0xefde, 0x1523, 0x785feabcd133)

// Define the dummy data structure
typedef struct {
    uint8_t deadzone;
    uint8_t up_report[8];
    uint8_t up_identPerRev;
    uint8_t up_transport;
    uint8_t dn_report[8];
    uint8_t dn_identPerRev;
    uint8_t dn_transport;
} rev_config_t;

// Define the stats data structure
typedef struct {
    uint32_t quat_data[4]; // Quaternion data (floats packed as uint32_t)
    uint16_t rotation_value;
} rev_stats_t;

extern rev_config_t config;
extern rev_stats_t stats;


// Public functions
void generate_random_stats_data(rev_stats_t *stats);
void rev_svc_loop(void);
void suspend_revsvc(void);

// int rev_send_gyro(float r, float i, float j, float k);


#endif // REVSVC_H