#ifndef BLE_H
#define BLE_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <hid_usage.h>
#include <hid_usage_pages.h>
#include <bluetooth/services/hids.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/dis.h>
#include <zephyr/bluetooth/services/bas.h>
#include <math.h>

extern bool connectd;

void enableBle(void);
void startAdv(void);
void deleteBond(void);

#endif // BLE_H
