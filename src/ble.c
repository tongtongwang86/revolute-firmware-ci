#include <zephyr/device.h>
#include <zephyr/init.h>

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include <zephyr/settings/settings.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci_types.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/addr.h>
#include "power.h"
#include <ble.h>

#if IS_ENABLED(CONFIG_SETTINGS)
#include <zephyr/settings/settings.h>
#endif

LOG_MODULE_REGISTER(ble, LOG_LEVEL_DBG);



// Initialize with a zeroed address
static bt_addr_le_t active_profile_addr = { 0 };

// Function to get the active profile address
bt_addr_le_t *zmk_ble_active_profile_addr(void) {
    return &active_profile_addr;
}


// Function to remove a bond
int remove_bonded_device(void) {

    // Try to unpair the device
    int err= bt_unpair(BT_ID_DEFAULT,BT_ADDR_LE_ANY);
    if (err) {
        LOG_ERR("Failed to unpair device %s (err %d)", err);
        return err;
    }

    update_advertising();

    return 0;
}


static int bond_count;

enum advertising_type advertising_status = ADV_FILTER;

#define CURR_ADV(adv) (adv << 4)

#define ADV_CONN_NAME                                                                          \
    BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_ONE_TIME | BT_LE_ADV_OPT_USE_NAME |  \
                        BT_LE_ADV_OPT_FORCE_NAME_IN_AD,                                            \
                    BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL)

#define ADV_FILTERED_NAME                                                                      \
    BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_ONE_TIME | BT_LE_ADV_OPT_USE_NAME |  \
                        BT_LE_ADV_OPT_FORCE_NAME_IN_AD | BT_LE_ADV_OPT_FILTER_CONN |               \
                        BT_LE_ADV_OPT_FILTER_SCAN_REQ,                                             \
                    BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL)
                

static uint8_t active_profile;

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

BUILD_ASSERT(DEVICE_NAME_LEN <= 16, "ERROR: BLE device name is too long. Max length: 16");

static struct bt_data zmk_ble_ad[] = {
    BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE, 0xCD, 0x04),
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_SOME, 0x12, 0x18, /* HID Service */
                  0x0f, 0x18                       /* Battery Service */
                  ),
};

static const struct bt_data rev_ble_sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_128_ENCODE(0x00001523, 0x1212, 0xefde, 0x1523, 0x785feabcd133)),
};

static void add_bonded_addr_to_filter_list(const struct bt_bond_info *info, void *data)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_le_filter_accept_list_add(&info->addr);
	bt_addr_le_to_str(&info->addr, addr_str, sizeof(addr_str));
	printk("Added %s to advertising accept filter list\n", addr_str);
	bond_count++;
}

bool active_profile_bonded(void) {

    bond_count = 0;
	bt_foreach_bond(BT_ID_DEFAULT, add_bonded_addr_to_filter_list, NULL);

    if (bond_count) {
        return true;
    } else {
        return false;
    }

}


static bool is_connected = false;

static void conn_check(struct bt_conn *conn, void *data) {
    struct bt_conn_info info;

    if (bt_conn_get_info(conn, &info) == 0) {
        if (info.state == BT_CONN_STATE_CONNECTED) {  // Ensure it's actively connected
            is_connected = true;
        }
    }
}

bool active_profile_connected(void) {
    is_connected = false;
    bt_conn_foreach(BT_CONN_TYPE_LE, conn_check, NULL); // Check all LE connections
    return is_connected;
}

#define CHECKED_ADV_STOP()                                                                         \
    err = bt_le_adv_stop();                                                                        \
    advertising_status = ADV_NONE;                                                             \
    LOG_DBG("advertising stopped");                                                                \
    if (err) {                                                                                     \
        LOG_ERR("Failed to stop advertising (err %d)", err);                                       \
        return err;                                                                                \
    }                                                          

#define CHECKED_FILTER_ADV()                                                                      \
    err = bt_le_adv_start(ADV_FILTERED_NAME, zmk_ble_ad, ARRAY_SIZE(zmk_ble_ad), rev_ble_sd, ARRAY_SIZE(rev_ble_sd)); \
    LOG_DBG("Advertising with filter enabled");                                                  \
    target_state = STATE_ADVERTISEMENT;                                                          \
    if (err) {                                                                                   \
        LOG_ERR("Filtered advertising failed to start (err %d)", err);                           \
        return err;                                                                              \
    }                                                                                            \
    advertising_status = ADV_FILTER;


#define CHECKED_OPEN_ADV()                                                                         \
    err = bt_le_adv_start(ADV_CONN_NAME, zmk_ble_ad, ARRAY_SIZE(zmk_ble_ad), rev_ble_sd, ARRAY_SIZE(rev_ble_sd));         \
    LOG_DBG("Advertising open");                                                                   \
    target_state = STATE_ADVERTISEMENT;                                                            \
    if (err) {                                                                                     \
        LOG_ERR("Advertising failed to start (err %d)", err);                                      \
        return err;                                                                                \
    }                                                                                              \
    advertising_status = ADV_CONN;                                                             
    



    int update_advertising(void) {
    
        int err = 0;
        bt_addr_le_t *addr;
        struct bt_conn *conn;
        enum advertising_type desired_adv = ADV_NONE;
    
        bond_count = 0;
        bt_foreach_bond(BT_ID_DEFAULT, add_bonded_addr_to_filter_list, NULL);

        if (active_profile_bonded()) {
            LOG_INF("Bonded");
            
            if (!active_profile_connected()) {
                LOG_INF("Bonded not connected");
                desired_adv = ADV_FILTER;
            }else{
                LOG_INF("Bonded and connected");
                desired_adv = ADV_NONE;
            }

        }else{
            desired_adv = ADV_CONN;
        }

        LOG_DBG("advertising from %d to %d", advertising_status, desired_adv);
    
        switch (desired_adv + CURR_ADV(advertising_status)) {
        case ADV_NONE + CURR_ADV(ADV_FILTER):
            CHECKED_ADV_STOP();
            break;
        case ADV_NONE + CURR_ADV(ADV_CONN):
            CHECKED_ADV_STOP();
            break;
        case ADV_FILTER + CURR_ADV(ADV_FILTER):
        case ADV_FILTER + CURR_ADV(ADV_CONN):
            CHECKED_ADV_STOP();
            CHECKED_FILTER_ADV();
            break;
        case ADV_FILTER + CURR_ADV(ADV_NONE):
            CHECKED_FILTER_ADV();
            break;
        case ADV_CONN + CURR_ADV(ADV_FILTER):
            CHECKED_ADV_STOP();
            CHECKED_OPEN_ADV();
            break;
        case ADV_CONN + CURR_ADV(ADV_NONE):
            CHECKED_OPEN_ADV();
            break;
        }
    
        return 0;
    };

    static void update_advertising_callback(struct k_work *work) { update_advertising(); }

K_WORK_DEFINE(update_advertising_work, update_advertising_callback);


int zmk_ble_set_device_name(char *name) {
    // Copy new name to advertising parameters
    int err = bt_set_name(name);
    LOG_DBG("New device name: %s", name);
    if (err) {
        LOG_ERR("Failed to set new device name (err %d)", err);
        return err;
    }
    if (advertising_status == ADV_CONN) {
        // Stop current advertising so it can restart with new name
        err = bt_le_adv_stop();
        advertising_status = ADV_NONE;
        if (err) {
            LOG_ERR("Failed to stop advertising (err %d)", err);
            return err;
        }
    }
    return update_advertising();
}




static void zmk_ble_ready(int err) {
    LOG_DBG("ready? %d", err);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return;
    }

    update_advertising();
}




static void connected(struct bt_conn *conn, uint8_t err) {
    char addr[BT_ADDR_LE_STR_LEN];
    struct bt_conn_info info;
    LOG_DBG("Connected thread: %p", k_current_get());

    bt_conn_get_info(conn, &info);

    if (info.role != BT_CONN_ROLE_PERIPHERAL) {
        LOG_DBG("SKIPPING FOR ROLE %d", info.role);
        return;
    }

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    advertising_status = ADV_NONE;
    

    if (err) {
        LOG_WRN("Failed to connect to %s (%u)", addr, err);
        update_advertising();
        return;
    }

    LOG_DBG("Connected %s", addr);


    update_advertising();


}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    char addr[BT_ADDR_LE_STR_LEN];
    struct bt_conn_info info;

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_DBG("Disconnected from %s (reason 0x%02x)", addr, reason);
    bt_conn_get_info(conn, &info);

    if (info.role != BT_CONN_ROLE_PERIPHERAL) {
        LOG_DBG("SKIPPING FOR ROLE %d", info.role);
        return;
    }

    // We need to do this in a work callback, otherwise the advertising update will still see the
    // connection for a profile as active, and not start advertising yet.
    k_work_submit(&update_advertising_work);

}

static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (!err) {
        LOG_DBG("Security changed: %s level %u", addr, level);
    } else {
        LOG_ERR("Security failed: %s level %u err %d", addr, level, err);
    }

}

static void le_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency,
                             uint16_t timeout) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_DBG("%s: interval %d latency %d timeout %d", addr, interval, latency, timeout);

}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
    .security_changed = security_changed,
    .le_param_updated = le_param_updated,
};



static void auth_cancel(struct bt_conn *conn) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));


    LOG_DBG("Pairing cancelled: %s", addr);

}


static enum bt_security_err auth_pairing_accept(struct bt_conn *conn,
                                                const struct bt_conn_pairing_feat *const feat) {
    struct bt_conn_info info;
    bt_conn_get_info(conn, &info);

    LOG_DBG("role %d", info.role);


    return BT_SECURITY_ERR_SUCCESS;
};

static void auth_pairing_complete(struct bt_conn *conn, bool bonded) {
    struct bt_conn_info info;
    char addr[BT_ADDR_LE_STR_LEN];
    const bt_addr_le_t *dst = bt_conn_get_dst(conn);

    bt_addr_le_to_str(dst, addr, sizeof(addr));
    bt_conn_get_info(conn, &info);

    if (info.role != BT_CONN_ROLE_PERIPHERAL) {
        LOG_DBG("SKIPPING FOR ROLE %d", info.role);
        return;
    }
    update_advertising();
};

static struct bt_conn_auth_cb zmk_ble_auth_cb_display = {
    .cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb zmk_ble_auth_info_cb_display = {
    .pairing_complete = auth_pairing_complete,
};

static int zmk_ble_complete_startup(void) {


    bt_conn_cb_register(&conn_callbacks);
    bt_conn_auth_cb_register(&zmk_ble_auth_cb_display);
    bt_conn_auth_info_cb_register(&zmk_ble_auth_info_cb_display);

    zmk_ble_ready(0);

    return 0;
}



static int zmk_ble_init(void) {

    LOG_INF("Bluetooth init");

    int err = bt_enable(NULL);

    if (err < 0 && err != -EALREADY) {
        LOG_ERR("BLUETOOTH FAILED (%d)", err);
        return err;
    }

#if IS_ENABLED(CONFIG_SETTINGS)
    settings_load();
    #endif
    zmk_ble_complete_startup();


    return 0;
}

void disable_bluetooth(void) {
    int err = bt_le_adv_stop();
    if (err) {
        printk("Failed to stop advertising (err %d)\n", err);
    } else {
        printk("Bluetooth advertising stopped.\n");
    }
	
	 err = bt_disable();
    if (err) {
        printk("Failed to disable Bluetooth (err %d)\n", err);
    } else {
        printk("Bluetooth disabled.\n");
    }
}

SYS_INIT(zmk_ble_init, APPLICATION, 50);