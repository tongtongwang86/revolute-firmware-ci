// #include "batterylvl.h"
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include "batterylvl.h"
#include <errno.h>

#define STACKSIZE 1024
#define PRIORITY 7

/* State of charge at or below which the device shuts itself down. */
#define BATTERY_EMPTY_PERCENT 2



LOG_MODULE_REGISTER(BatteryLevel, LOG_LEVEL_INF);
K_THREAD_STACK_DEFINE(batteryUpdateThread_stack_area, STACKSIZE);
static struct k_thread batteryUpdateThread_data;
const struct device *const bq = DEVICE_DT_GET_ONE(ti_bq274xx);

/* Forward declarations */
int getbatterylevel(const struct device *dev);
void batteryUpdateThread(void *p1, void *p2, void *p3);


/* The gauge is read from the sensor thread, which is the thread that owns the
 * I2C bus and knows when it is powered. This thread only brings the gauge back
 * out of shutdown at boot and then exits. */
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

    /* The gauge may have been left in shutdown mode by the previous power_off().
     * Shutdown is only exited by pulsing GPOUT, which is what RESUME does. */
    int pm_ret = pm_device_action_run(bq, PM_DEVICE_ACTION_RESUME);

    if (pm_ret && pm_ret != -EALREADY && pm_ret != -ENOTSUP) {
        LOG_WRN("Failed to wake fuel gauge (err %d)", pm_ret);
    }

    LOG_INF("Fuel gauge ready");
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

int battery_read_percent(void) {
    if (bq == NULL || !device_is_ready(bq)) {
        return -ENODEV;
    }

    int level = getbatterylevel(bq);

    /* getbatterylevel() reports 0 on a failed read, which must not be confused
     * with a genuinely flat cell. */
    return (level <= 0) ? -EIO : level;
}

void battery_publish(int percent) {
    if (percent < 0) {
        return;
    }

    int err = bt_bas_set_battery_level((uint8_t)percent);

    if (err) {
        LOG_INF("Can't send battery report, err: %d", err);
    }
}

bool battery_level_is_empty(int percent) {
    return (percent > 0) && (percent <= BATTERY_EMPTY_PERCENT);
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
        return false;
    }

    int level = getbatterylevel(bq);

    /* getbatterylevel() reports 0 when the read itself failed, so a failed read
     * must not be mistaken for a flat cell and trigger a shutdown. */
    if (level <= 0) {
        return false;
    }

    return level <= BATTERY_EMPTY_PERCENT;
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

void battery_shutdown(void) {
    if (bq == NULL || !device_is_ready(bq)) {
        return;
    }

    /* TURN_OFF maps to the driver's shutdown-mode sequence. Left in its normal
     * operating mode the BQ27427 draws ~100uA from the cell forever, which on
     * its own is ~100x the System OFF budget for the whole board. */
    int ret = pm_device_action_run(bq, PM_DEVICE_ACTION_TURN_OFF);

    if (ret && ret != -EALREADY) {
        LOG_WRN("Failed to shut down fuel gauge (err %d)", ret);
    } else {
        LOG_INF("Fuel gauge in shutdown mode");
    }
}

SYS_INIT(batteryThreadinit, APPLICATION, 50);