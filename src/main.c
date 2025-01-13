
/*
 * Copyright (c) 2022 Michal Morsisko
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/gpio.h>
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
#include <zephyr/logging/log.h>
#include "hog.h"
#include "revsvc.h"

#define LED0_NODE DT_ALIAS(led0)
#define SW3_NODE DT_ALIAS(sw3)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec sw3 = GPIO_DT_SPEC_GET(SW3_NODE, gpios);

#define BUTTON_THREAD_STACK_SIZE 1024
#define BUTTON_THREAD_PRIORITY 5

K_THREAD_STACK_DEFINE(button_thread_stack, BUTTON_THREAD_STACK_SIZE);
struct k_thread button_thread_data;
k_tid_t button_thread_tid;

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static struct bt_le_adv_param adv_param;
static bt_addr_le_t bond_addr;

static void advertising_start(struct k_work *work);
static K_WORK_DEFINE(start_advertising_worker, advertising_start);

// Define thread stack sizes and priorities
#define REV_SVC_THREAD_STACK_SIZE 1024  // Adjust based on requirements
#define HOG_BUTTON_THREAD_STACK_SIZE 1024  // Adjust based on requirements
#define REV_SVC_THREAD_PRIORITY 5  // Priority for rev_svc_loop thread
#define HOG_BUTTON_THREAD_PRIORITY 5  // Priority for hog_button_loop thread

// Thread stack and control blocks
K_THREAD_STACK_DEFINE(rev_svc_stack, REV_SVC_THREAD_STACK_SIZE); 
K_THREAD_STACK_DEFINE(hog_button_stack, HOG_BUTTON_THREAD_STACK_SIZE); 
static struct k_thread rev_svc_thread_data;
static struct k_thread hog_button_thread_data;


void button_thread_fn(void *arg1, void *arg2, void *arg3) {
    int ret;

    if (!device_is_ready(sw3.port)) {
        LOG_ERR("Button device %s not ready", sw3.port->name);
        return;
    }

    ret = gpio_pin_configure_dt(&sw3, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("Failed to configure button GPIO (err %d)", ret);
        return;
    }

    // Enable pull-up or pull-down (if needed)
    ret = gpio_pin_interrupt_configure_dt(&sw3, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure button interrupt (err %d)", ret);
        return;
    }

    LOG_INF("Button thread started");

    while (1) {
        if (gpio_pin_get_dt(&sw3)) {
            LOG_INF("Button pressed! Initiating Bluetooth unpair...");
            bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);

            // Debounce delay
            k_sleep(K_MSEC(500));
        }

        k_sleep(K_MSEC(100));  // Polling delay
    }
}


// Thread entry function for rev_svc_loop
void rev_svc_thread(void *arg1, void *arg2, void *arg3) {
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    rev_svc_loop();
}

// Thread entry function for hog_button_loop
void hog_button_thread(void *arg1, void *arg2, void *arg3) {
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    hog_button_loop();
}

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
	    // Initialize LED GPIO
    if (!device_is_ready(led.port)) {
        LOG_ERR("LED device %s not ready", led.port->name);
        return;
    }

    int err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (err < 0) {
        LOG_ERR("Failed to configure LED GPIO (err %d)", err);
        return;
    }
 	err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (err < 0) {
        LOG_ERR("Failed to configure LED GPIO (err %d)", err);
        return;
    }


	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	bt_ready();
	bt_conn_auth_info_cb_register(&bt_conn_auth_info);
	    // Create a thread for the rev_svc_loop function
 
    // Create thread for rev_svc_loop
    k_thread_create(&rev_svc_thread_data, rev_svc_stack,
                    K_THREAD_STACK_SIZEOF(rev_svc_stack),
                    rev_svc_thread,
                    NULL, NULL, NULL,
                    REV_SVC_THREAD_PRIORITY,
                    0,
                    K_NO_WAIT);

    LOG_INF("rev_svc_loop thread started\n");

    // Create thread for hog_button_loop
    k_thread_create(&hog_button_thread_data, hog_button_stack,
                    K_THREAD_STACK_SIZEOF(hog_button_stack),
                    hog_button_thread,
                    NULL, NULL, NULL,
                    HOG_BUTTON_THREAD_PRIORITY,
                    0,
                    K_NO_WAIT);

	    button_thread_tid = k_thread_create(&button_thread_data, button_thread_stack,
                                        K_THREAD_STACK_SIZEOF(button_thread_stack),
                                        button_thread_fn, NULL, NULL, NULL,
                                        BUTTON_THREAD_PRIORITY, 0, K_NO_WAIT);

	// hog_button_loop();

	while (1) {
		  gpio_pin_toggle_dt(&led);  // Toggle LED state
        k_sleep(K_MSEC(500));     // Sleep for 500 ms
	}

	
	
	return 0;
}