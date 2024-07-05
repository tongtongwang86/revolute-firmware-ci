

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_backend_ble.h>

#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

LOG_MODULE_REGISTER(ble_backend);


static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, LOGGER_BACKEND_BLE_ADV_UUID_DATA)
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void start_adv(void)
{	
	    struct bt_le_adv_param *adv_param = BT_LE_ADV_PARAM(
	        BT_LE_ADV_OPT_CONNECTABLE, 
	        BT_GAP_ADV_FAST_INT_MIN_2,
	        BT_GAP_ADV_FAST_INT_MAX_2,
	        NULL
	    );
	
	int err;
	err = bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return;
	}

	LOG_INF("Advertising successfully started");
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err 0x%02x)", err);
	} else {
		LOG_INF("Connected");
	}

		if (bt_conn_set_security(conn, BT_SECURITY_L2)) {
		LOG_INF("Failed to set security\n");
	}
	
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{	
	int err;
	LOG_INF("Disconnected (reason 0x%02x)", reason);
	start_adv();
	
	 err = bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
			if (err) {
				LOG_INF("Cannot delete bond (err: %d)\n", err);
			} else {
				LOG_INF("Bond deleted succesfully");
			}
	
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_INF("Security changed: %s level %u\n", addr, level);
	} else {
		LOG_INF("Security failed: %s level %u err %d\n", addr, level,
		       err);
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
};

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing cancelled: %s", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.cancel = auth_cancel,
};

void backend_ble_hook(bool status, void *ctx)
{
	ARG_UNUSED(ctx);

	if (status) {
		LOG_INF("BLE Logger Backend enabled.");
	} else {
		LOG_INF("BLE Logger Backend disabled.");
	}
}


int main(void)
{
	int err;

	LOG_INF("BLE LOG Demo");
	logger_backend_ble_set_hook(backend_ble_hook, NULL);
	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return 0;
	}

	bt_conn_auth_cb_register(&auth_cb_display);

	start_adv();

	while (1) {
		uint32_t uptime_secs = k_uptime_get_32()/1000U;

		LOG_INF("Uptime %d secs", uptime_secs);
		k_sleep(K_MSEC(1000));
	}
	return 0;
}
