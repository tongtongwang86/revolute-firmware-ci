#ifndef BLE_H
#define BLE_H

#include <zephyr/kernel.h>
#include <stdio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/settings/settings.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include "power.h"

#define BT_LE_ADV_CONN_NO_ACCEPT_LIST                                                              \
	BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_ONE_TIME,                        \
			BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL)

#define BT_LE_ADV_CONN_ACCEPT_LIST                                                                 \
	BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_FILTER_CONN |                    \
				BT_LE_ADV_OPT_ONE_TIME,                                            \
			BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL)

static int bond_count;

static struct bt_le_adv_param adv_param;


static struct bt_le_adv_param *adv_param_normal = BT_LE_ADV_PARAM(
	(BT_LE_ADV_OPT_CONN |
	 BT_LE_ADV_OPT_USE_IDENTITY), /* Connectable advertising and use identity address */
	BT_GAP_ADV_FAST_INT_MIN_1,                          /* Min Advertising Interval 500ms (800*0.625ms) */
	BT_GAP_ADV_FAST_INT_MAX_1,                          /* Max Advertising Interval 500.625ms (801*0.625ms) */
	NULL);                        /* Set to NULL for undirected advertising */

static bt_addr_le_t bond_addr;

static struct bt_le_adv_param *adv_param_directed = BT_LE_ADV_CONN_DIR_LOW_DUTY(&bond_addr);




/* Vendor Primary Service Declaration */

static const struct bt_data ad[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_128_ENCODE(0x00001523, 0x1212, 0xefde, 0x1523, 0x785feabcd133)),
};

void bluetooth_init(void);

void bluetooth_adv(void);

void disable_bluetooth(void);

#endif // BLE_H