#include "revsvc.h"
#include "settings.h"
#include "ble.h"
#include "magnetic.h"

LOG_MODULE_REGISTER(RevSVC, LOG_LEVEL_DBG);

rev_config_t config = {
    .deadzone = 0x00,
    .up_report = {0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00},
    .up_identPerRev = 0x1E,
    .up_transport = 0x0D, // 5 keyboard, 9 consumer, 13 mouse
    .dn_report = {0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00},
    .dn_identPerRev = 0x1E,
    .dn_transport = 0x0D // 5 keyboard, 9 consumer, 13 mouse
};


static uint8_t stats_notifications_enabled;
// Initialize the dummy data


rev_stats_t stats = {
    .quat_data = {0x3f800000, 0x00000000, 0x00000000, 0x00000000}, // Quaternion identity (1.0, 0.0, 0.0, 0.0)
    .rotation_value = 0x0001 // Example rotation value
};



static void rev_ccc_stats_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    stats_notifications_enabled = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
    LOG_INF("%d\n",stats_notifications_enabled);


}


// Callback to read the characteristic data
static ssize_t read_callback_config(struct bt_conn *conn,
                                    const struct bt_gatt_attr *attr,
                                    void *buf, uint16_t len, uint16_t offset) {
    const uint8_t *value = (const uint8_t *)&config;
    size_t value_len = sizeof(config);

    if (offset >= value_len) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    size_t to_copy = MIN(len, value_len - offset);
    memcpy(buf, value + offset, to_copy);

    return to_copy;
}

static ssize_t write_callback_config(struct bt_conn *conn,
                                     const struct bt_gatt_attr *attr,
                                     const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    uint8_t *value = (uint8_t *)&config;
    size_t value_len = sizeof(config);

    if (offset >= value_len) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    size_t to_copy = MIN(len, value_len - offset);
    memcpy(value + offset, buf, to_copy);

    // Print the updated values to the console
    LOG_INF("Updated config:");
    LOG_INF("deadzone: %u", config.deadzone);
    LOG_INF("up_report: %02x %02x %02x %02x %02x %02x %02x %02x", 
            config.up_report[0], config.up_report[1], config.up_report[2], config.up_report[3], 
            config.up_report[4], config.up_report[5], config.up_report[6], config.up_report[7]);
    LOG_INF("up_identPerRev: %u", config.up_identPerRev);
    LOG_INF("up_transport: %u", config.up_transport);
    LOG_INF("dn_report: %02x %02x %02x %02x %02x %02x %02x %02x", 
            config.dn_report[0], config.dn_report[1], config.dn_report[2], config.dn_report[3], 
            config.dn_report[4], config.dn_report[5], config.dn_report[6], config.dn_report[7]);
    LOG_INF("dn_identPerRev: %u", config.dn_identPerRev);
    LOG_INF("dn_transport: %u", config.dn_transport);
    save_config();
    return to_copy;
}

// Define the GATT service and characteristic


void generate_clock_based_stats_data(rev_stats_t *stats) {
    // Use system uptime as a source of data
    uint32_t uptime_ms = k_uptime_get_32();

    // Generate deterministic quaternion values based on uptime
    float r = (float)(uptime_ms % 1000) / 1000.0f; // Scaled to [0, 1]
    float i = (float)((uptime_ms / 10) % 1000) / 1000.0f; // Scaled to [0, 1]
    float j = (float)((uptime_ms / 100) % 1000) / 1000.0f; // Scaled to [0, 1]
    float k = (float)((uptime_ms / 1000) % 1000) / 1000.0f; // Scaled to [0, 1]

    memcpy(&stats->quat_data[0], &r, sizeof(float));
    memcpy(&stats->quat_data[1], &i, sizeof(float));
    memcpy(&stats->quat_data[2], &j, sizeof(float));
    memcpy(&stats->quat_data[3], &k, sizeof(float));

    // Generate a rotation value based on uptime
    stats->rotation_value = angle; // Cycles between 0 and 359
}


static ssize_t write_callback_name(struct bt_conn *conn,
                                   const struct bt_gatt_attr *attr,
                                   const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    char name_buf[32]; // Ensure enough space for device name
    if (len >= sizeof(name_buf)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    
    memcpy(name_buf, buf, len);
    name_buf[len] = '\0'; // Null-terminate the string
    
    int err = zmk_ble_set_device_name(name_buf);
    if (err < 0) {
        LOG_ERR("Failed to set device name (err %d)", err);
        return err;
    }
    
    LOG_INF("Device name set to: %s", name_buf);
    return len;
}

// Read callback for the timer characteristic
static ssize_t read_callback_timer(struct bt_conn *conn,
    const struct bt_gatt_attr *attr,
    void *buf, uint16_t len, uint16_t offset) {
const uint8_t *value = (const uint8_t *)&timer;
size_t value_len = sizeof(timer);

if (offset >= value_len) {
return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
}

size_t to_copy = MIN(len, value_len - offset);
memcpy(buf, value + offset, to_copy);

return to_copy;
}

// Write callback for the timer characteristic
static ssize_t write_callback_timer(struct bt_conn *conn,
     const struct bt_gatt_attr *attr,
     const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
uint8_t *value = (uint8_t *)&timer;
size_t value_len = sizeof(timer);

if (offset >= value_len) {
return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
}

size_t to_copy = MIN(len, value_len - offset);
memcpy(value + offset, buf, to_copy);

// Log the updated timer values
LOG_INF("Updated timers:");
LOG_INF("autoofftimer: %u ms", timer.autoofftimer);
LOG_INF("autoFilterOffTimer: %u ms", timer.autoFilterOffTimer);

// Optionally save to flash
save_config();

return to_copy;
}


// Define the GATT service and characteristics
BT_GATT_SERVICE_DEFINE(rev_svc,
    BT_GATT_PRIMARY_SERVICE(REV_SVC_UUID),

    BT_GATT_CHARACTERISTIC(REV_READCONFIG_UUID, BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ_ENCRYPT, read_callback_config, NULL, NULL),

    BT_GATT_CHARACTERISTIC(REV_WRITECONFIG_UUID, BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE_ENCRYPT, NULL, write_callback_config, NULL),

    BT_GATT_CHARACTERISTIC(REV_STATS_UUID, BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ_ENCRYPT, NULL, NULL, NULL),

    BT_GATT_CCC(rev_ccc_stats_cfg_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),

    BT_GATT_CHARACTERISTIC(REV_WRITENAME_UUID, BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE_ENCRYPT, NULL, write_callback_name, NULL),

    BT_GATT_CHARACTERISTIC(REV_WRITETIMER_UUID, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                            BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT,
                            read_callback_timer, write_callback_timer, NULL)


);



void stream_stats_data(void) {
    // while (stats_notifications_enabled == 1) {
        generate_clock_based_stats_data(&stats);

        int err = bt_gatt_notify(NULL, &rev_svc.attrs[3], &stats, sizeof(stats));
        if (err) {
            LOG_ERR("Failed to send notification: %d", err);
        } else {
            LOG_INF("Stats notification sent");
        }

        k_sleep(K_MSEC(1000)); // Send notifications every 1 second
    // }
}

void rev_svc_loop(void)
{ 
for (;;) {
		if (stats_notifications_enabled) {
		
            generate_clock_based_stats_data(&stats);

            int err = bt_gatt_notify(NULL, &rev_svc.attrs[6], &stats, sizeof(stats));
        if (err) {
            LOG_ERR("Failed to send notification: %d", err);
        } else {
            LOG_INF("Stats notification sent");
        }
        
			// bt_gatt_notify(NULL, &rev_svc.attrs[3], &stats, sizeof(stats));

		}
		k_sleep(K_MSEC(100));
	}
}



void rev_svc_thread_init(void) {
    k_thread_create(&rev_svc_thread_data, rev_svc_stack,
                    K_THREAD_STACK_SIZEOF(rev_svc_stack),
                    (k_thread_entry_t)rev_svc_loop,
                    NULL, NULL, NULL,
                    REV_SVC_THREAD_PRIORITY, 0, K_NO_WAIT);



    LOG_INF("rev_svc_loop thread initialized");
}

SYS_INIT(rev_svc_thread_init, APPLICATION, 50);