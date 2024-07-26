#include "revsvc.h"
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(RevSVC, LOG_LEVEL_DBG);




struct config_profile config = {
    .mode = 5, // 5 for keyboard, 9 for consumer, 13 for mouse
    .identPerRev = 30,
    .deadzone = 0,
    .upReport = {0,0,0,0,0,0,0,HID_KEY_Z},
    .downReport = {0,0,0,0,0,0,0,HID_KEY_X},
    
};

static int settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
    if (strcmp(name, "config") == 0) {
        ssize_t read_len = read_cb(cb_arg, &config, sizeof(config));
        if (read_len == sizeof(config)) {
            LOG_INF("Loaded config from flash");
            return 0;
        }
    }
    return -ENOENT;
}

static int settings_get(const char *name, char *val, int val_len_max)
{
    if (strcmp(name, "config") == 0) {
        if (val_len_max < sizeof(config)) {
            return -ENOMEM;
        }
        memcpy(val, &config, sizeof(config));
        return sizeof(config);
    }
    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(my_module, "my_module", NULL, settings_set, settings_get, NULL);

void save_config(void)
{
    int rc = settings_save_one("my_module/config", &config, sizeof(config));
    if (rc) {
        LOG_ERR("Failed to save config (err %d)", rc);
    } else {
        LOG_INF("Config saved to flash");
    }
}

void load_config(void)
{
    int rc = settings_load();
    if (rc) {
        LOG_ERR("Failed to load settings (err %d)", rc);
    } else {
        LOG_INF("Settings loaded from flash");
    }
}


struct revieved_data {
    uint8_t mode;
    uint8_t resolution;
    uint8_t dead_zone;
    uint8_t upreport[8];
    uint8_t downreport[8];
};



uint16_t angle_value = 0; // This will hold the angle value to be read

ssize_t read_callback_angle(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    const uint16_t *value = attr->user_data;
    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(*value));
}

ssize_t write_callback_mode(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    uint8_t *value = attr->user_data;

    if (offset + len > sizeof(uint8_t)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    memcpy((uint8_t *)value + offset, buf, len);
    config.mode = *value;

    LOG_INF("Received mode: %02x\n", *value);
    save_config();
    return len;
}

ssize_t write_callback_resolution(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    uint8_t *value = attr->user_data;

    if (offset + len > sizeof(uint8_t)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    memcpy((uint8_t *)value + offset, buf, len);
    config.identPerRev = (int)(*value);

    LOG_INF("Received resolution: %d\n", (int)(*value));
    save_config();

    return len;
}

ssize_t write_callback_deadzone(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    uint8_t *value = attr->user_data;

    if (offset + len > sizeof(uint8_t)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    memcpy((uint8_t *)value + offset, buf, len);
    config.deadzone = *value;

    LOG_INF("Received dead zone: %02x\n", *value);

    save_config();

    return len;
}

ssize_t write_callback_upreport(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    uint8_t *value = attr->user_data;

    if (offset + len > 8) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    memcpy((uint8_t *)value + offset, buf, len);
    memcpy(config.upReport, value, 8);

    LOG_INF("Received up report: ");
    for (int i = 0; i < 8; i++) {
        LOG_INF("%02x ", config.upReport[i]);
    }
    LOG_INF("\n");
    save_config();

    return len;
}

ssize_t write_callback_downreport(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    uint8_t *value = attr->user_data;

    if (offset + len > 8) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    memcpy((uint8_t *)value + offset, buf, len);
    memcpy(config.downReport, value, 8);

    LOG_INF("Received down report: ");
    for (int i = 0; i < 8; i++) {
        LOG_INF("%02x ", config.downReport[i]);
    }
    LOG_INF("\n");
    save_config();

    return len;
}

BT_GATT_SERVICE_DEFINE(rev_svc,
    BT_GATT_PRIMARY_SERVICE(REV_SVC_UUID),

    BT_GATT_CHARACTERISTIC(REV_ANGLE_UUID, BT_GATT_CHRC_READ, BT_GATT_PERM_READ_ENCRYPT, read_callback_angle, NULL, &angle_value),

    BT_GATT_CHARACTERISTIC(REV_MODE_UUID, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE_ENCRYPT, NULL, write_callback_mode, &config.mode),

    BT_GATT_CHARACTERISTIC(REV_RESOLUTION_UUID, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE_ENCRYPT, NULL, write_callback_resolution, &config.identPerRev),

    BT_GATT_CHARACTERISTIC(REV_DEADZONE_UUID, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE_ENCRYPT, NULL, write_callback_deadzone, &config.deadzone),

    BT_GATT_CHARACTERISTIC(REV_UPREPORT_UUID, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE_ENCRYPT, NULL, write_callback_upreport, config.upReport),

    BT_GATT_CHARACTERISTIC(REV_DOWNREPORT_UUID, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE_ENCRYPT, NULL, write_callback_downreport, config.downReport),
);