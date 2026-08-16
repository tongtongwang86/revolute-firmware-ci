#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

#include "sensor.h"
#include "power.h"
#include "statemanager.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define WDT_FEED_INTERVAL_MS 1000  // Feed every second
#define WDT_TIMEOUT_MS 3000        // Timeout: 3 seconds

int main(void)
{
    LOG_INF("Revolute starting");

    sensor_init();

    const struct device *wdt_dev;
    int wdt_channel_id;
    struct wdt_timeout_cfg wdt_config;

    wdt_dev = DEVICE_DT_GET(DT_ALIAS(watchdog0));
    if (!device_is_ready(wdt_dev)) {
        LOG_ERR("Watchdog not ready");
        return -ENODEV;
    }

    wdt_config.flags = WDT_FLAG_RESET_SOC;
    wdt_config.window.min = 0;
    wdt_config.window.max = WDT_TIMEOUT_MS;
    wdt_config.callback = NULL;  // No interrupt handler, just reset on timeout

    wdt_channel_id = wdt_install_timeout(wdt_dev, &wdt_config);
    if (wdt_channel_id < 0) {
        LOG_ERR("Failed to install watchdog timeout (err %d)", wdt_channel_id);
        return wdt_channel_id;
    }

    /* Pause the watchdog while the CPU is asleep. The device spends most of its
     * life idle, and a sleeping CPU is not a hung one -- letting the counter run
     * through idle would just force periodic wakeups to feed it. */
    if (wdt_setup(wdt_dev, WDT_OPT_PAUSE_IN_SLEEP) < 0) {
        LOG_ERR("Failed to start watchdog");
        return -EIO;
    }

    while (1) {
        wdt_feed(wdt_dev, wdt_channel_id);
        k_msleep(WDT_FEED_INTERVAL_MS);
    }

    return 0;
}
