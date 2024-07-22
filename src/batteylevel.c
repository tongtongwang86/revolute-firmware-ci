#define STACKSIZE 1024
static struct k_thread batteryUpdateThread_data;

void batteryUpdateThread()
{
    struct sensor_value voltage, current, state_of_charge,
        full_charge_capacity, remaining_charge_capacity, avg_power,
        int_temp, current_standby, current_max_load, state_of_health;

    const struct device *const bq = DEVICE_DT_GET_ONE(ti_bq274xx);

    if (!device_is_ready(bq))
    {
        printk("Device %s is not ready\n", bq->name);
        return 0;
    }

    printk("device is %p, name is %s\n", bq, bq->name);

    uint8_t level;

    while (1)
    {

        level = getbatterylevel(bq);

        LOG_INF("State of charge: %d%%\n", level);

        int err;
        err = bt_bas_set_battery_level(level);
        if (err)
        {
            // LOG_INF("cant send battery report", err);
            return 0;
        }

        k_sleep(K_MSEC(1000));
    }
}



int getbatterylevel(const struct device *dev)
{

    int status = 0;
    struct sensor_value state_of_charge;

    status = sensor_sample_fetch_chan(dev,
                                      SENSOR_CHAN_GAUGE_STATE_OF_CHARGE);
    if (status < 0)
    {
        printk("Unable to fetch State of Charge\n");
        return;
    }

    status = sensor_channel_get(dev,
                                SENSOR_CHAN_GAUGE_STATE_OF_CHARGE,
                                &state_of_charge);
    if (status < 0)
    {
        printk("Unable to get state of charge\n");
        return;
    }

    return state_of_charge.val1;
}



void batteryThreadinit()
{
K_THREAD_STACK_DEFINE(batteryUpdateThread_stack_area, STACKSIZE);
k_thread_create(&batteryUpdateThread_data, batteryUpdateThread_stack_area,
                K_THREAD_STACK_SIZEOF(batteryUpdateThread_stack_area),
                batteryUpdateThread, NULL, NULL, NULL,
                PRIORITY, 0, K_FOREVER);
k_thread_name_set(&batteryUpdateThread_data, "batterythread");

k_thread_start(&batteryUpdateThread_data);

}