#include "gpio.h"

const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int ledInit()
{
    int ret;
    if (!gpio_is_ready_dt(&led))
    {
        return 0;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0)
    {
        return 0;
    }
    return ret;
}
