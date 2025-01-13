#include "revsvc.h"

LOG_MODULE_REGISTER(RevSVC, LOG_LEVEL_DBG);


// Initialize the dummy data
static rev_config_t config = {
    .deadzone = 0x01,
    .up_report = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17},
    .up_identPerRev = 0x02,
    .up_transport = 0x03,
    .dn_report = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27},
    .dn_identPerRev = 0x04,
    .dn_transport = 0x05
};

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

    return to_copy;
}

// Define the GATT service and characteristic
BT_GATT_SERVICE_DEFINE(rev_svc,
    BT_GATT_PRIMARY_SERVICE(REV_SVC_UUID),

    BT_GATT_CHARACTERISTIC(REV_READCONFIG_UUID, BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ_ENCRYPT, read_callback_config, NULL, NULL),

    BT_GATT_CHARACTERISTIC(REV_WRITECONFIG_UUID, BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE_ENCRYPT, NULL, write_callback_config, NULL),
);