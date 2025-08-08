#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/drivers/charger.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_charger, LOG_LEVEL_INF);

// Change to match your DTS label
#define CHARGER_NODE DT_NODELABEL(bq25180)

static int charger_sysinit(const struct device *dev)
{
    ARG_UNUSED(dev);

    const struct device *charger = DEVICE_DT_GET(CHARGER_NODE);

    if (!device_is_ready(charger)) {
        LOG_ERR("BQ25180 charger device not ready");
        return -ENODEV;
    }

    union charger_propval val;

    // Set charging current to 60 mA (60000 µA)
    val.const_charge_current_ua = 60000;

    int ret = charger_set_prop(charger, CHARGER_PROP_CONSTANT_CHARGE_CURRENT_UA, &val);
    if (ret < 0) {
        LOG_ERR("Failed to set charging current (err %d)", ret);
    } else {
        LOG_INF("Charging current set to %d µA", val.const_charge_current_ua);
    }

    // Enable charging
    ret = charger_charge_enable(charger, true);
    if (ret < 0) {
        LOG_ERR("Failed to enable charger (err %d)", ret);
    } else {
        LOG_INF("Charger enabled");
    }

    return 0;
}

// Priority should be AFTER charger driver init
SYS_INIT(charger_sysinit, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
