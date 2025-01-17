#include "magnetic.h"
#include "revsvc.h"

LOG_MODULE_REGISTER(spin, LOG_LEVEL_DBG);
static struct k_thread print_thread_data;
static K_THREAD_STACK_DEFINE(print_stack, THREAD_STACK_SIZE);


int as5600_refresh(const struct device *dev)
{
    int ret;
    struct sensor_value rot_raw;
    ret = sensor_sample_fetch_chan(dev, SENSOR_CHAN_ROTATION);
    if (ret != 0)
    {
        printk("Sample fetch error: %d\n", ret);
        return ret;
    }
    ret = sensor_channel_get(dev, SENSOR_CHAN_ROTATION, &rot_raw);
    if (ret != 0)
    {
        printk("Sensor channel get error: %d\n", ret);
        return ret;
    }

    int degrees = rot_raw.val1;

    return degrees;
}


// Thread entry point
static void print_thread(void *unused1, void *unused2, void *unused3)
{

    LOG_INF("Print thread started");

    const struct device *const as = DEVICE_DT_GET(DT_INST(0, ams_as5600));

    if (as == NULL || !device_is_ready(as))
    {
        printk("as5600 device tree not configured\n");
        return;
    }

    int new_degree = as5600_refresh(as);

    while (1) {
        new_degree = as5600_refresh(as);

        k_sleep(K_MSEC(100));
        LOG_INF("%d\n",new_degree);
    }
}

void SensorThreadinit(void)
{
    // Start the print thread
    k_thread_create(&print_thread_data, print_stack, K_THREAD_STACK_SIZEOF(print_stack),
                    print_thread, NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    return 0;
}
