// #include "batterylvl.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include "batterylvl.h"

#define STACKSIZE 1024
#define PRIORITY 7



LOG_MODULE_REGISTER(BatteryLevel, LOG_LEVEL_INF);
K_THREAD_STACK_DEFINE(batteryUpdateThread_stack_area, STACKSIZE);
static struct k_thread batteryUpdateThread_data;
const struct device *const bq = DEVICE_DT_GET_ONE(ti_bq274xx);

/* Forward declarations */
int getbatterylevel(const struct device *dev);
void batteryUpdateThread(void *p1, void *p2, void *p3);


void batteryUpdateThread(void *p1, void *p2, void *p3) {

    if (bq == NULL) {
        printk("Battery device not found (bq is NULL)\n");
        return;
    }

    if (!device_is_ready(bq)) {
        /* Avoid dereferencing bq->name when device isn't ready */
        printk("Battery device is not ready\n");
        return;
    }

    printk("Device is %p, name is %s\n", bq, bq->name);

    uint8_t level;

    while (1) {
        level = getbatterylevel(bq);

        LOG_INF("State of charge: %d%%", level);

        int err = bt_bas_set_battery_level(level);
        if (err) {
            LOG_INF("Can't send battery report, err: %d", err);
            return;
        }

        k_sleep(K_MSEC(5000));
    }
}

int getbatterylevel(const struct device *dev) {
    int status;
    struct sensor_value state_of_charge;

    status = sensor_sample_fetch_chan(dev, SENSOR_CHAN_GAUGE_STATE_OF_CHARGE);
    if (status < 0) {
        printk("Unable to fetch State of Charge\n");
        return 0;
    }

    status = sensor_channel_get(dev, SENSOR_CHAN_GAUGE_STATE_OF_CHARGE, &state_of_charge);
    if (status < 0) {
        printk("Unable to get state of charge\n");
        return 0;
    }

    return state_of_charge.val1;
}

void sendbattery(void) {
  uint8_t level;
  level = getbatterylevel(bq);
  LOG_INF("State of charge: %d%%\n", level);

  int err = bt_bas_set_battery_level(level);
  if (err) {
      LOG_INF("Can't send battery report, err: %d\n", err);
      return;
  }
}

bool is_battery_empty(void) {

    if (!device_is_ready(bq)) {
        printk("Device %s is not ready\n", bq->name);
        return false;
    }

    int level = getbatterylevel(bq);
    if (level < 0) {
        return true;
    }
    return false;
}





void batteryThreadinit(void) {
    k_thread_create(&batteryUpdateThread_data, batteryUpdateThread_stack_area,
                    K_THREAD_STACK_SIZEOF(batteryUpdateThread_stack_area),
                    batteryUpdateThread, NULL, NULL, NULL,
                    PRIORITY, 0, K_NO_WAIT);

}

void battery_stop(void) {
    /* Abort the battery update thread to stop periodic fuel-gauge reads */
    k_thread_abort(&batteryUpdateThread_data);
}

SYS_INIT(batteryThreadinit, APPLICATION, 50);