
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
#include "batterylvl.h"
#include "button.h"

static int bond_count;

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);


LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static struct bt_le_adv_param adv_param;
static bt_addr_le_t bond_addr;

static void advertising_start(struct k_work *work);
static K_WORK_DEFINE(start_advertising_worker, advertising_start);

// Define thread stack sizes and priorities

#define HOG_BUTTON_THREAD_STACK_SIZE 1024  // Adjust based on requirements

#define HOG_BUTTON_THREAD_PRIORITY 5  // Priority for hog_button_loop thread

// Thread stack and control blocks

K_THREAD_STACK_DEFINE(hog_button_stack, HOG_BUTTON_THREAD_STACK_SIZE); 
// static struct k_thread rev_svc_thread_data;
static struct k_thread hog_button_thread_data;

#define BT_LE_ADV_CONN_NO_ACCEPT_LIST                                                              \
	BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_ONE_TIME,                        \
			BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL)

#define BT_LE_ADV_CONN_ACCEPT_LIST                                                                 \
	BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_FILTER_CONN |                    \
				BT_LE_ADV_OPT_ONE_TIME,                                            \
			BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL)


static void setup_accept_list_cb(const struct bt_bond_info *info, void *user_data)
{
	int *bond_cnt = user_data;

	if ((*bond_cnt) < 0) {
		return;
	}

	int err = bt_le_filter_accept_list_add(&info->addr);
	LOG_INF("Added following peer to accept list: %x %x\n", info->addr.a.val[0],
		info->addr.a.val[1]);
	if (err) {
		LOG_INF("Cannot add peer to filter accept list (err: %d)\n", err);
		(*bond_cnt) = -EIO;
	} else {
		(*bond_cnt)++;
	}
}

static int setup_accept_list(uint8_t local_id)
{
	int err = bt_le_filter_accept_list_clear();

	if (err) {
		LOG_INF("Cannot clear accept list (err: %d)\n", err);
		return err;
	}

	int bond_cnt = 0;

	bt_foreach_bond(local_id, setup_accept_list_cb, &bond_cnt);

	return bond_cnt;
}

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

void advertise_with_acceptlist(struct k_work *work)
{
	int err = 0;
	int allowed_cnt = setup_accept_list(BT_ID_DEFAULT);
	if (allowed_cnt < 0) {
		LOG_INF("Acceptlist setup failed (err:%d)\n", allowed_cnt);
	} else {
		if (allowed_cnt == 0) {
			LOG_INF("Advertising with no Accept list \n");
			err = bt_le_adv_start(BT_LE_ADV_CONN_NO_ACCEPT_LIST, ad, ARRAY_SIZE(ad), sd,
					      ARRAY_SIZE(sd));
		} else {
			LOG_INF("Acceptlist setup number  = %d \n", allowed_cnt);
			err = bt_le_adv_start(BT_LE_ADV_CONN_ACCEPT_LIST, ad, ARRAY_SIZE(ad), sd,
					      ARRAY_SIZE(sd));
		}
		if (err) {
			LOG_INF("Advertising failed to start (err %d)\n", err);
			return;
		}
		LOG_INF("Advertising successfully started\n");
	}
}
K_WORK_DEFINE(advertise_acceptlist_work, advertise_with_acceptlist);







static struct bt_le_adv_param *adv_param_normal = BT_LE_ADV_PARAM(
	(BT_LE_ADV_OPT_CONN |
	 BT_LE_ADV_OPT_USE_IDENTITY), /* Connectable advertising and use identity address */
	BT_GAP_ADV_FAST_INT_MIN_1,                          /* Min Advertising Interval 500ms (800*0.625ms) */
	BT_GAP_ADV_FAST_INT_MAX_1,                          /* Max Advertising Interval 500.625ms (801*0.625ms) */
	NULL);                        /* Set to NULL for undirected advertising */

static struct bt_le_adv_param *adv_param_directed = BT_LE_ADV_CONN_DIR_LOW_DUTY(&bond_addr);


static void copy_last_bonded_addr(const struct bt_bond_info *info, void *data)
{
	bt_addr_le_copy(&bond_addr, &info->addr);
}

static void add_bonded_addr_to_filter_list(const struct bt_bond_info *info, void *data)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_le_filter_accept_list_add(&info->addr);
	bt_addr_le_to_str(&info->addr, addr_str, sizeof(addr_str));
	printk("Added %s to advertising accept filter list\n", addr_str);
	bond_count++;
}

static void advertising_start(struct k_work *work)
{
	int err;
	
	bond_count = 0;
	bt_foreach_bond(BT_ID_DEFAULT, add_bonded_addr_to_filter_list, NULL);

	adv_param = *BT_LE_ADV_CONN_FAST_1;

	/* If we have got at least one bond, activate the filter */
	if (bond_count) {
		/* BT_LE_ADV_OPT_FILTER_CONN is required to activate accept filter list,
		 * BT_LE_ADV_OPT_FILTER_SCAN_REQ will prevent sending scan response data to
		 * devices, that are not on the accept filter list
		 */
		adv_param.options |= BT_LE_ADV_OPT_FILTER_CONN | BT_LE_ADV_OPT_FILTER_SCAN_REQ;
	}

	err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

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
		// k_work_submit(&start_advertising_worker);
		k_work_submit(&advertise_acceptlist_work);
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
	// k_work_submit(&start_advertising_worker);
	// printk("Advertising started\n");
	k_work_submit(&advertise_acceptlist_work);

}

static void on_security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_INF("Security changed: %s level %u\n", addr, level);
	} else {
		LOG_INF("Security failed: %s level %u err %d\n", addr, level, err);
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = on_security_changed
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

		if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	// k_work_submit(&start_advertising_worker);
	k_work_submit(&advertise_acceptlist_work);


	bt_conn_auth_info_cb_register(&bt_conn_auth_info);


    LOG_INF("rev_svc_loop thread started\n");

    // Create thread for hog_button_loop
    k_thread_create(&hog_button_thread_data, hog_button_stack,
                    K_THREAD_STACK_SIZEOF(hog_button_stack),
                    hog_button_thread,
                    NULL, NULL, NULL,
                    HOG_BUTTON_THREAD_PRIORITY,
                    0,
                    K_NO_WAIT);

	    // button_thread_tid = k_thread_create(&button_thread_data, button_thread_stack,
        //                                 K_THREAD_STACK_SIZEOF(button_thread_stack),
        //                                 button_thread_fn, NULL, NULL, NULL,
        //                                 BUTTON_THREAD_PRIORITY, 0, K_NO_WAIT);

	batteryThreadinit();
	rev_svc_thread_init();
	rev_button_thread_init();
	// hog_button_loop();

	while (1) {
		  gpio_pin_toggle_dt(&led);  // Toggle LED state
        k_sleep(K_MSEC(500));     // Sleep for 500 ms
	}

	
	
	return 0;
}