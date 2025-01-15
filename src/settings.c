#include "settings.h"
#include "revsvc.h"

LOG_MODULE_REGISTER(Settings, LOG_LEVEL_DBG);

// initial configuration

static int settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
    if (strcmp(name, "config") == 0) {
        ssize_t read_len = read_cb(cb_arg, &config, sizeof(config));  // This should now work
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
        if (val_len_max < sizeof(config)) {  // This should now work
            return -ENOMEM;
        }
        memcpy(val, &config, sizeof(config));  // This should now work
        return sizeof(config);
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