#include <zephyr/kernel.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/printk.h>

#define WDT_FEED_INTERVAL_MS 1000  // Feed every second
#define WDT_TIMEOUT_MS 3000        // Timeout: 3 seconds


// #include "hog2.h"

void main(void)
{


    sensor_init();
	// hog_init();
	 const struct device *wdt_dev;
    int wdt_channel_id;
    struct wdt_timeout_cfg wdt_config;

    // printk("nRF52 Watchdog Demo Start\n");

    wdt_dev = DEVICE_DT_GET(DT_ALIAS(watchdog0));
    if (!device_is_ready(wdt_dev)) {
        printk("Watchdog not ready!\n");
        return;
    }

    wdt_config.flags = WDT_FLAG_RESET_SOC;
    wdt_config.window.min = 0;
    wdt_config.window.max = WDT_TIMEOUT_MS;
    wdt_config.callback = NULL;  // No interrupt handler, just reset on timeout

    wdt_channel_id = wdt_install_timeout(wdt_dev, &wdt_config);
    if (wdt_channel_id < 0) {
        printk("Failed to install watchdog timeout!\n");
        return;
    }

    if (wdt_setup(wdt_dev, 0) < 0) {
        printk("Failed to start watchdog!\n");
        return;
    }

    while (1) {
        // printk("Feeding watchdog...\n");
        wdt_feed(wdt_dev, wdt_channel_id);
        k_msleep(WDT_FEED_INTERVAL_MS);
    }
	
	// hog_button_loop() now runs in its own thread
}
