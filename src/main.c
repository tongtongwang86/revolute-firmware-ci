
#include <zephyr/drivers/gpio.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>
#include "hog.h"
#include "revsvc.h"
#include "batterylvl.h"
#include "button.h"
#include "ble.h"
#include "settings.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);



#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);










int main(void)
{
	    // Initialize LED GPIO
    if (!device_is_ready(led.port)) {
        LOG_ERR("LED device %s not ready", led.port->name);
        return;
    }

    int err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (err < 0) {
        LOG_ERR("Failed to configure LED GPIO (err %d)", err);
        return;
    }
 	err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (err < 0) {
        LOG_ERR("Failed to configure LED GPIO (err %d)", err);
        return;
    }



	bluetooth_init();


    LOG_INF("rev_svc_loop thread started\n");


	batteryThreadinit();
	rev_svc_thread_init();
	button_init();


	while (1) {
		  gpio_pin_toggle_dt(&led);  // Toggle LED state
        k_sleep(K_MSEC(500));     // Sleep for 500 ms
	}

	
	
	return 0;
}