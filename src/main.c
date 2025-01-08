
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>
#include <zephyr/linker/linker-defs.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(Revolute, LOG_LEVEL_DBG);
/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 100

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* Register advertising data */
static const struct bt_data ad[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL,
                  BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_128_ENCODE(0x00001523, 0x1212, 0xefde, 0x1523, 0x785feabcd133)),
};
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

	/* Enable Bluetooth */
	ret = bt_enable(NULL);
	if (ret < 0)
	{
		printk("Bluetooth init failed (err %d)\n", ret);
		return ret;
	}

	/* Start advertising */
	ret = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
						  sd, ARRAY_SIZE(sd));
	if (ret < 0)
	{
		printk("Advertising failed to start (err %d)\n", ret);
		return ret;
	}

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