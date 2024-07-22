#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include "ble.h"
#include "gpio.h"

LOG_MODULE_REGISTER(Revolute, LOG_LEVEL_DBG);

int main(void)
{
    ledInit();
    enableBle();
    startAdv();

    while (1)
    {
        int err = gpio_pin_toggle_dt(&led);
        if (err < 0)
        {
            return 0;
        }

        LOG_INF("a");

        k_msleep(1000);
    }

    return 0;
}
