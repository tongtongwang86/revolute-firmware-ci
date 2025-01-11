
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/linker/linker-defs.h>
#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>
// #include <zephyr/logging/log.h>

#include "ble.h"

bool connectd = false;

// LOG_MODULE_REGISTER(Revolute, LOG_LEVEL_DBG);
/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 100

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);


int main(void)
{
	int ret;

	printk("hoi Board: %s\n", CONFIG_BOARD);
	printk("Build time: " __DATE__ " " __TIME__ "\n");
	printk("Address of sample %p\n", (void *)__rom_region_start);

	if (!gpio_is_ready_dt(&led))
	{
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0)
	{
		return -ENODEV;
	}

	enableBle();
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