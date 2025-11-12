#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/drivers/charger.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_charger, LOG_LEVEL_INF);

#define CHARGER_NODE DT_NODELABEL(bq25180)
#define CHARGER_CHECK_INTERVAL_MS 5000 // Check every 5 seconds

static const struct device *charger_dev;

// Function to log charger status
static void log_charger_status(void)
{
    union charger_propval val;
    int ret = charger_get_prop(charger_dev, CHARGER_PROP_STATUS, &val);
    if (ret < 0) {
        LOG_ERR("Failed to get charger status (err %d)", ret);
        return;
    }

    switch (val.status) {
    case CHARGER_STATUS_UNKNOWN:
        LOG_INF("Charger status: Unknown");
        break;
    case CHARGER_STATUS_CHARGING:
        LOG_INF("Charger status: Charging");
        break;
    case CHARGER_STATUS_NOT_CHARGING:
        LOG_INF("Charger status: Not charging");
        break;
    case CHARGER_STATUS_FULL:
        LOG_INF("Charger status: Full");
        break;
    default:
        LOG_INF("Charger status: %d", val.status);
        break;
    }
}

// Thread function to periodically check charger status
static void charger_status_thread(void)
{
    while (1) {
        log_charger_status();
        k_sleep(K_MSEC(CHARGER_CHECK_INTERVAL_MS));
    }
}

// Define the thread stack and control block
K_THREAD_DEFINE(charger_status_tid, 1024, charger_status_thread, NULL, NULL, NULL,
                K_PRIO_COOP(7), 0, 0);

// System init function
static int charger_sysinit(const struct device *dev)
{
    ARG_UNUSED(dev);

    charger_dev = DEVICE_DT_GET(CHARGER_NODE);
    if (!device_is_ready(charger_dev)) {
        LOG_ERR("BQ25180 charger device not ready");
        return -ENODEV;
    }

    union charger_propval val;

    // Set charging current
    val.const_charge_current_ua = 60000;
    int ret = charger_set_prop(charger_dev, CHARGER_PROP_CONSTANT_CHARGE_CURRENT_UA, &val);
    if (ret < 0) {
        LOG_ERR("Failed to set charging current (err %d)", ret);
    } else {
        LOG_INF("Charging current set to %d µA", val.const_charge_current_ua);
    }

    // Enable charging
    ret = charger_charge_enable(charger_dev, true);
    if (ret < 0) {
        LOG_ERR("Failed to enable charger (err %d)", ret);
    } else {
        LOG_INF("Charger enabled");
    }

    return 0;
}

// Init after charger driver
SYS_INIT(charger_sysinit, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

void charger_stop(void) {
    if (charger_dev != NULL && device_is_ready(charger_dev)) {
        int ret = charger_charge_enable(charger_dev, false);
        if (ret < 0) {
            LOG_ERR("Failed to disable charger (err %d)", ret);
        } else {
            LOG_INF("Charger disabled");
        }
    }

    /* Stop the charger status thread */
    k_thread_abort(charger_status_tid);
}
