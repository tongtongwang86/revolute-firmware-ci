#include "revsvc.h"

LOG_MODULE_REGISTER(RevSVC, LOG_LEVEL_DBG);


static ssize_t read_callback_config(struct bt_conn *conn,
                                    const struct bt_gatt_attr *attr,
                                    void *buf, uint16_t len, uint16_t offset) {
    ssize_t to_copy;

    // Copy the config profile data to the buffer
    to_copy = MIN(len, sizeof(config) - offset);
    if (to_copy > 0) {
        memcpy(buf, ((uint8_t *)&config) + offset, to_copy);
    }

    return to_copy;
}


BT_GATT_SERVICE_DEFINE(rev_svc,
    BT_GATT_PRIMARY_SERVICE(REV_SVC_UUID),

    BT_GATT_CHARACTERISTIC(REV_READCONFIG_UUID, BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ_ENCRYPT, read_callback_config, NULL, NULL),




);

