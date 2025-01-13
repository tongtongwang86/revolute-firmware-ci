
/*
 * Copyright (c) 2022 Michal Morsisko
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include "hog.h"
#include "revsvc.h"

static struct bt_le_adv_param adv_param;
static bt_addr_le_t bond_addr;

static void advertising_start(struct k_work *work);
static K_WORK_DEFINE(start_advertising_worker, advertising_start);


static struct bt_le_adv_param *adv_param_normal = BT_LE_ADV_PARAM(
	(BT_LE_ADV_OPT_CONN |
	 BT_LE_ADV_OPT_USE_IDENTITY), /* Connectable advertising and use identity address */
	BT_GAP_ADV_FAST_INT_MIN_1,                          /* Min Advertising Interval 500ms (800*0.625ms) */
	BT_GAP_ADV_FAST_INT_MAX_1,                          /* Max Advertising Interval 500.625ms (801*0.625ms) */
	NULL);                        /* Set to NULL for undirected advertising */

static struct bt_le_adv_param *adv_param_directed = BT_LE_ADV_CONN_DIR_LOW_DUTY(&bond_addr);

/* Vendor Primary Service Declaration */

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void copy_last_bonded_addr(const struct bt_bond_info *info, void *data)
{
	bt_addr_le_copy(&bond_addr, &info->addr);
}

static void advertising_start(struct k_work *work)
{
	int err;
	char addr[BT_ADDR_LE_STR_LEN];
	
	bt_addr_le_copy(&bond_addr, BT_ADDR_LE_NONE);
	bt_foreach_bond(BT_ID_DEFAULT, copy_last_bonded_addr, NULL);

	/* Address is equal to BT_ADDR_LE_NONE if compare returns 0.
	 * This means there is no bond yet.
	 */
	if (bt_addr_le_cmp(&bond_addr, BT_ADDR_LE_NONE) != 0) {
		bt_addr_le_to_str(&bond_addr, addr, sizeof(addr));
		printk("Direct advertising to %s\n", addr);

		//adv_param = *BT_LE_ADV_CONN_DIR_LOW_DUTY(&bond_addr);
		adv_param = *BT_LE_ADV_CONN_DIR(&bond_addr);
		adv_param.options |= BT_LE_ADV_OPT_DIR_ADDR_RPA;
		err = bt_le_adv_start(adv_param_directed, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	} else {
		err = bt_le_adv_start(adv_param_normal, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	}

	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
	} else {
		printk("Advertising successfully started\n");
	}	
}

static void bt_ready(void)
{
	int err;
	char addr[BT_ADDR_LE_STR_LEN];

	printk("Bluetooth initialized\n");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	bt_addr_le_copy(&bond_addr, BT_ADDR_LE_NONE);
	bt_foreach_bond(BT_ID_DEFAULT, copy_last_bonded_addr, NULL);

	/* Address is equal to BT_ADDR_LE_NONE if compare returns 0.
	 * This means there is no bond yet.
	 */

	if (bt_addr_le_cmp(&bond_addr, BT_ADDR_LE_NONE) != 0) {
		bt_addr_le_to_str(&bond_addr, addr, sizeof(addr));
		printk("Direct advertising to %s\n", addr);

		// adv_param = *BT_LE_ADV_CONN_DIR(&bond_addr);
		// adv_param.options |= BT_LE_ADV_OPT_DIR_ADDR_RPA;
		err = bt_le_adv_start(adv_param_directed, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	} else {
		// err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), NULL, 0);
		err = bt_le_adv_start(adv_param_normal, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	}

	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
	} else {
		printk("Advertising successfully started\n");
	}
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err == BT_HCI_ERR_ADV_TIMEOUT) {
		printk("Trying to re-start directed adv.\n");
		k_work_submit(&start_advertising_worker);
	}
	else if (err) {
		printk("Connection failed (err 0x%02x)\n", err);
	} else {
		printk("Connected\n");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected, reason 0x%02x %s\n", reason, bt_hci_err_to_str(reason));
	k_work_submit(&start_advertising_worker);
	printk("Advertising started\n");
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected
};


void pairing_complete(struct bt_conn *conn, bool bonded)
{
	printk("Pairing completed\n");

}

static struct bt_conn_auth_info_cb bt_conn_auth_info = {
	.pairing_complete = pairing_complete
};

int main(void)
{

	k_sleep(K_SECONDS(1));
	int err;

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	bt_ready();
	bt_conn_auth_info_cb_register(&bt_conn_auth_info);

	hog_button_loop();
	return 0;
}