#include "settings.h"
#include "revsvc.h"

LOG_MODULE_REGISTER(Settings, LOG_LEVEL_DBG);

// initial configuration

static int settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
    if (strcmp(name, "config") == 0) {
        ssize_t read_len = read_cb(cb_arg, &config, sizeof(config));
        if (read_len == sizeof(config)) {
            LOG_INF("Loaded config from flash");
            return 0;
        }
    } else if (strcmp(name, "rev_timer") == 0) {
        ssize_t read_len = read_cb(cb_arg, &timer, sizeof(timer));
        if (read_len == sizeof(timer)) {
            LOG_INF("Loaded timer from flash");
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
    else if (strcmp(name, "rev_timer") == 0) {
        if (val_len_max < sizeof(timer)) {
            return -ENOMEM;
        }
        memcpy(val, &timer, sizeof(timer));
        return sizeof(timer);
    }
    return -ENOENT;
}

// Define settings handler
SETTINGS_STATIC_HANDLER_DEFINE(rev_module, "rev_module", NULL, settings_set, settings_get, NULL);

// Functions to load/save config
void save_config(void)
{
    int rc = settings_save_one("rev_module/config", &config, sizeof(config));
    if (rc) {
        LOG_ERR("Failed to save config (err %d)", rc);
    } else {
        LOG_INF("Config saved to flash");
    }

    rc = settings_save_one("rev_module/rev_timer", &timer, sizeof(timer));
    if (rc) {
        LOG_ERR("Failed to save timer settings (err %d)", rc);
    } else {
        LOG_INF("Timer settings saved to flash");
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