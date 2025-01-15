
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


// static void advertising_start(struct k_work *work);
// static K_WORK_DEFINE(start_advertising_worker, advertising_start);

// Define thread stack sizes and priorities

#define HOG_BUTTON_THREAD_STACK_SIZE 1024  // Adjust based on requirements

#define HOG_BUTTON_THREAD_PRIORITY 5  // Priority for hog_button_loop thread

// Thread stack and control blocks

K_THREAD_STACK_DEFINE(hog_button_stack, HOG_BUTTON_THREAD_STACK_SIZE); 
static struct k_thread hog_button_thread_data;





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


	// err = bt_enable(NULL);
	// if (err) {
	// 	printk("Bluetooth init failed (err %d)\n", err);
	// 	return 0;
	// }

	// 	if (IS_ENABLED(CONFIG_SETTINGS)) {
	// 	settings_load();
	// }

	// // k_work_submit(&start_advertising_worker);
	// k_work_submit(&advertise_acceptlist_work);


	// bt_conn_auth_info_cb_register(&bt_conn_auth_info);

	bluetooth_init();


    LOG_INF("rev_svc_loop thread started\n");

    // Create thread for hog_button_loop
    k_thread_create(&hog_button_thread_data, hog_button_stack,
                    K_THREAD_STACK_SIZEOF(hog_button_stack),
                    hog_button_thread,
                    NULL, NULL, NULL,
                    HOG_BUTTON_THREAD_PRIORITY,
                    0,
                    K_NO_WAIT);

	    // button_thread_tid = k_thread_create(&button_thread_data, button_thread_stack,
        //                                 K_THREAD_STACK_SIZEOF(button_thread_stack),
        //                                 button_thread_fn, NULL, NULL, NULL,
        //                                 BUTTON_THREAD_PRIORITY, 0, K_NO_WAIT);

	batteryThreadinit();
	rev_svc_thread_init();
	rev_button_thread_init();
	// hog_button_loop();

	while (1) {
		  gpio_pin_toggle_dt(&led);  // Toggle LED state
        k_sleep(K_MSEC(500));     // Sleep for 500 ms
	}

	
	
	return 0;
}