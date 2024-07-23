#include "revsvc.h"

LOG_MODULE_REGISTER(RevSVC, LOG_LEVEL_DBG);


uint16_t read_value = 0;

struct config_profile config = {
    .mode = 13, // 5 for keyboard, 9 for consumer, 13 for mouse
    .identPerRev = 12,
    .deadzone = 2,
    .upReport = {0,0,0,1,0,0,0,0},
    .downReport = {0,0,0,1,0,0,0,0},
    
};

ssize_t read_callback(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    const uint16_t *value = attr->user_data;
    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(*value));
}

ssize_t write_callback(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    uint8_t *value = attr->user_data;

    if (offset + len > sizeof(*value)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    memcpy(value + offset, buf, len);

    // Print the received data
    printk("Received data: ");
    for (int i = 0; i < len; i++) {
        printk("%02x ", ((uint8_t *)buf)[i]);
    }
    printk("\n");

    return len;
}

BT_GATT_SERVICE_DEFINE(rev_svc,
    BT_GATT_PRIMARY_SERVICE(REV_SVC_UUID),
    BT_GATT_CHARACTERISTIC(REV_READ_UUID, BT_GATT_CHRC_READ, BT_GATT_PERM_READ_ENCRYPT, read_callback, NULL, &read_value),
    BT_GATT_CHARACTERISTIC(REV_WRITE_UUID, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE_ENCRYPT, NULL, write_callback, NULL),
);