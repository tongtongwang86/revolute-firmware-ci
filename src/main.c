
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
// #include <zephyr/mgmt/mcumgr/transport/smp_bt.h>
#include <zephyr/linker/linker-defs.h>
#include <zephyr/logging/log.h>

#include "ble.h"
// #include "settings.h"
// #include "revsvc.h"
// #include "gpio.h"

#define LED0_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

LOG_MODULE_REGISTER(Revolute, LOG_LEVEL_DBG);

// struct config_profile config = {
// 	.deadzone = 0,
// 	.up_report = {0, 0, 0, 0, 1, 0, 0, 0},
// 	.up_identPerRev = 30,
// 	.up_transport = 3,
// 	.dn_report = {0, 0, 0, 0, 1, 0, 0, 0},
// 	.dn_identPerRev = 30,
// 	.dn_transport = 3,
// };


/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000


// static void button_event_handler(size_t idx, enum button_evt evt)
// {
// 	int err;
// 	switch (idx)
// 	{
// 	case 0:
// 		if (evt == BUTTON_EVT_PRESSED)
// 		{

// 			LOG_INF("Button 0 pressed");
// 		}
// 		else
// 		{
// 			LOG_INF("Button 0 released");
// 		}
// 		break;
// 	case 1:
// 		if (evt == BUTTON_EVT_PRESSED)
// 		{
// 			err = gpio_pin_toggle_dt(&led);
// 			if (err < 0)
// 			{
// 				return;
// 			}

// 			LOG_INF("Button 1 pressed");
// 		}
// 		else
// 		{

// 			LOG_INF("Button 1 released");
// 		}
// 		break;
// 	case 2:
// 		if (evt == BUTTON_EVT_PRESSED)
// 		{

// 			LOG_INF("Button 2 pressed");
// 		}
// 		else
// 		{

// 			LOG_INF("Button 2 released");
// 		}
// 		break;
// 	case 3:
// 		if (evt == BUTTON_EVT_PRESSED)
// 		{
// 			deleteBond();
// 			LOG_INF("Button 3 pressed");
// 		}
// 		else
// 		{
// 			LOG_INF("Button 3 released");
// 		}
// 		break;
// 	default:
// 		LOG_ERR("Unknown button %zu event", idx);
// 		break;
// 	}
// }


int main(void)
{
	int ret;

	printk("Board: %s\n", CONFIG_BOARD);
	printk("Build time: " __DATE__ " " __TIME__ "\n");
	printk("Address of sample %p\n", (void *)__rom_region_start);

	// for (size_t i = 0; i < 4; i++)
	// {
	// 	button_init(i, button_event_handler);
	// }
	// ledInit();

	 ret;
    if (!gpio_is_ready_dt(&led)) {
        return 0;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return 0;
    }
    return ret;


	enableBle();
	// load_config();
	// save_config();
	startAdv();
	while (1)
	{
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0)
		{
			return -ENODEV;
		}
		k_msleep(SLEEP_TIME_MS);
	}

	return 0;
}