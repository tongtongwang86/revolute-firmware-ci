#include "ble.h"

// LOG_MODULE_REGISTER(BLE, LOG_LEVEL_DBG);
static K_SEM_DEFINE(ble_init_ok, 0, 1);
struct bt_conn *my_connection;


/* Register advertising data */
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

static void connected(struct bt_conn *conn, uint8_t err)
{
	struct bt_conn_info info;
	char addr[BT_ADDR_LE_STR_LEN];

	my_connection = conn;

	if (err)
	{
		printk("oof%u\n", err);
		return;
	}
	else if (bt_conn_get_info(conn, &info))
	{
		printk("no info\n");
	}
	else
	{
		bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

		printk("%s,%u,%u,%u,%u\n",
			   addr, info.role, info.le.interval, info.le.latency, info.le.timeout);
	}
}

// What happens when the device is disconnected
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	// printk("discon %u\n", reason);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
							 enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err)
	{
		printk("Security changed: level %u\n", level);
	}
	else
	{
		printk("Security failed: level %u err %d\n", level, err);
	}
}

// Callback for BLE update request
static bool le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param)
{
	// If acceptable params, return true, otherwise return false.
	return true;
}

// Callback for BLE parameter update
static void le_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency, uint16_t timeout)
{
	struct bt_conn_info info;
	char addr[BT_ADDR_LE_STR_LEN];

	if (bt_conn_get_info(conn, &info))
	{
		printk("Could not parse connection info\n");
	}
	else
	{
		bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

		printk("Connection parameters updated!	\n\
		Connected to: %s						\n\
		New Connection Interval: %u				\n\
		New Slave Latency: %u					\n\
		New Connection Supervisory Timeout: %u	\n",
			   addr, info.le.interval, info.le.latency, info.le.timeout);
	}
}

// Wire up all the callbacks we defined above to the bt struct
static struct bt_conn_cb conn_callbacks =
	{
		.connected = connected,
		.disconnected = disconnected,
		.security_changed = security_changed,
		.le_param_req = le_param_req,
		.le_param_updated = le_param_updated,
};

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled\n");
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing completed\n");
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing failed, reason %d\n", reason);
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
	.passkey_display = NULL,
	.passkey_confirm = NULL,
	.cancel = auth_cancel,
	.pairing_confirm = NULL};

// This will be called to initialize our bluetooth stack
static void bt_ready(int err)
{

	if (err)
	{
		printk("BLE init failed with error code %d\n", err);
		return;
	}

	// Configure connection callbacks
	bt_conn_cb_register(&conn_callbacks);
	// printk("Security Enabled\n");
	bt_conn_auth_cb_register(&conn_auth_callbacks);

	settings_load();

	// Initialize services
	// err = led_service_init();

	if (err)
	{
		printk("Failed to init lightbox (err:%d)\n", err);
		return;
	}

	// Start advertising
	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
						  sd, ARRAY_SIZE(sd));
	if (err)
	{
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");


}


void enableBle(void){

    int err = bt_enable(bt_ready);


	if (err)
	{
		printk("Failed to init lightbox (err:%d)\n", err);
		return;
	}

}


void resetBond(void){

    int ret = bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
printk("Attempting to unpair device %d\n", ret);



}