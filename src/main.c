
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* Advertising parameters */
static struct bt_le_adv_param adv_param = {
    .options = (BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_IDENTITY),
    .interval_min = 800,
    .interval_max = 801,
    .peer = NULL,
};

/* Advertising data */
static const struct bt_data ad[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL,
                  BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

/* Connection callbacks */
static void connected(struct bt_conn *conn, uint8_t err) {
    char addr[BT_ADDR_LE_STR_LEN];
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("Connected: %s", addr);
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("Disconnected: %s (reason 0x%02x)", addr, reason);
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

/* Authentication callbacks */
static void pairing_confirm(struct bt_conn *conn) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("Pairing confirmation requested for %s", addr);
    bt_conn_auth_pairing_confirm(conn);  // Automatically accept pairing
}

static struct bt_conn_auth_cb auth_callbacks = {
    .pairing_confirm = pairing_confirm,
    .passkey_display = NULL,
    .passkey_entry = NULL,
    .passkey_confirm = NULL,
    .oob_data_request = NULL,
    .cancel = NULL,
};

/* Start advertising */
static void start_advertising(void) {
    int err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Advertising failed to start (err %d)", err);
        return;
    }
    LOG_INF("Advertising successfully started");
}

/* Settings handlers */
static void load_settings(void) {
    int err = settings_load();
    if (err) {
        LOG_ERR("Failed to load settings (err %d)", err);
    } else {
        LOG_INF("Settings loaded successfully");
    }
}

/* Bluetooth ready callback */
static void bt_ready(int err) {
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return;
    }

    LOG_INF("Bluetooth initialized");
    
}

/* Main application */
int main(void) {
    int err;

    LOG_INF("Starting Bluetooth application");

    /* Initialize Bluetooth subsystem */
    err = bt_enable(bt_ready);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return -1;
    }
	load_settings();
    /* Register connection and authentication callbacks */
    bt_conn_cb_register(&conn_callbacks);
    bt_conn_auth_cb_register(&auth_callbacks);
	start_advertising();
    /* Load settings for persistent storage */
    

    return 0;
}
