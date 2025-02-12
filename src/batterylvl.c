#include "batterylvl.h"

LOG_MODULE_REGISTER(BatteryLevel, LOG_LEVEL_INF);
K_THREAD_STACK_DEFINE(batteryUpdateThread_stack_area, STACKSIZE);
static struct k_thread batteryUpdateThread_data;
const struct device *const bq = DEVICE_DT_GET_ONE(ti_bq274xx);


void batteryUpdateThread(void) {
    struct sensor_value voltage, current, state_of_charge,
        full_charge_capacity, remaining_charge_capacity, avg_power,
        int_temp, current_standby, current_max_load, state_of_health;


    if (!device_is_ready(bq)) {
        printk("Device %s is not ready\n", bq->name);
        return;
    }

    printk("Device is %p, name is %s\n", bq, bq->name);

    uint8_t level;

    while (1) {
        level = getbatterylevel(bq);

        LOG_INF("State of charge: %d%%\n", level);

        int err = bt_bas_set_battery_level(level);
        if (err) {
            LOG_INF("Can't send battery report, err: %d\n", err);
            return;
        }

        k_sleep(K_MSEC(5000));
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

static void bq274xx_show_values(const char *type, struct sensor_value value)
{
	if ((value.val2 < 0) && (value.val1 >= 0)) {
		value.val2 = -(value.val2);
		printk("%s: -%d.%06d\n", type, value.val1, value.val2);
	} else if ((value.val2 > 0) && (value.val1 < 0)) {
		printk("%s: %d.%06d\n", type, value.val1, value.val2);
	} else if ((value.val2 < 0) && (value.val1 < 0)) {
		value.val2 = -(value.val2);
		printk("%s: %d.%06d\n", type, value.val1, value.val2);
	} else {
		printk("%s: %d.%06d\n", type, value.val1, value.val2);
	}
}

void printbatterystats(void){
    
    int status = 0;

    struct sensor_value voltage, current, state_of_charge,
    full_charge_capacity, remaining_charge_capacity, avg_power,
    int_temp, current_standby, current_max_load, state_of_health;



    if (!device_is_ready(bq)) {
        printk("Device %s is not ready\n", bq->name);
        return;
    }

    status = sensor_sample_fetch_chan(bq,
        SENSOR_CHAN_GAUGE_VOLTAGE);
if (status < 0) {
printk("Unable to fetch the voltage\n");
return;
}

status = sensor_channel_get(bq, SENSOR_CHAN_GAUGE_VOLTAGE,
      &voltage);
if (status < 0) {
printk("Unable to get the voltage value\n");
return;
}

printk("Voltage: %d.%06dV\n", voltage.val1, voltage.val2);

status = sensor_sample_fetch_chan(bq,
         SENSOR_CHAN_GAUGE_AVG_CURRENT);
if (status < 0) {
printk("Unable to fetch the Average current\n");
return;
}

status = sensor_channel_get(bq, SENSOR_CHAN_GAUGE_AVG_CURRENT,
      &current);
if (status < 0) {
printk("Unable to get the current value\n");
return;
}

bq274xx_show_values("Avg Current in Amps", current);

status = sensor_sample_fetch_chan(bq,
  SENSOR_CHAN_GAUGE_STDBY_CURRENT);
if (status < 0) {
printk("Unable to fetch Standby Current\n");
return;
}

status = sensor_channel_get(bq,
  SENSOR_CHAN_GAUGE_STDBY_CURRENT,
  &current_standby);
if (status < 0) {
printk("Unable to get the current value\n");
return;
}

bq274xx_show_values("Standby Current in Amps", current_standby);

status = sensor_sample_fetch_chan(bq,
  SENSOR_CHAN_GAUGE_MAX_LOAD_CURRENT);
if (status < 0) {
printk("Unable to fetch Max Load Current\n");
return;
}

status = sensor_channel_get(bq,
  SENSOR_CHAN_GAUGE_MAX_LOAD_CURRENT,
  &current_max_load);
if (status < 0) {
printk("Unable to get the current value\n");
return;
}

bq274xx_show_values("Max Load Current in Amps",
  current_max_load);

status = sensor_sample_fetch_chan(bq,
  SENSOR_CHAN_GAUGE_STATE_OF_CHARGE);
if (status < 0) {
printk("Unable to fetch State of Charge\n");
return;
}

status = sensor_channel_get(bq,
      SENSOR_CHAN_GAUGE_STATE_OF_CHARGE,
      &state_of_charge);
if (status < 0) {
printk("Unable to get state of charge\n");
return;
}

printk("State of charge: %d%%\n", state_of_charge.val1);

status = sensor_sample_fetch_chan(bq,
  SENSOR_CHAN_GAUGE_STATE_OF_HEALTH);
if (status < 0) {
printk("Failed to fetch State of Health\n");
return;
}

status = sensor_channel_get(bq,
      SENSOR_CHAN_GAUGE_STATE_OF_HEALTH,
      &state_of_health);
if (status < 0) {
printk("Unable to get state of charge\n");
return;
}

printk("State of health: %d%%\n", state_of_health.val1);

status = sensor_sample_fetch_chan(bq,
  SENSOR_CHAN_GAUGE_AVG_POWER);
if (status < 0) {
printk("Unable to fetch Avg Power\n");
return;
}

status = sensor_channel_get(bq, SENSOR_CHAN_GAUGE_AVG_POWER,
      &avg_power);
if (status < 0) {
printk("Unable to get avg power\n");
return;
}

bq274xx_show_values("Avg Power in Watt", avg_power);

status = sensor_sample_fetch_chan(bq,
SENSOR_CHAN_GAUGE_FULL_CHARGE_CAPACITY);
if (status < 0) {
printk("Failed to fetch Full Charge Capacity\n");
return;
}

status = sensor_channel_get(bq,
SENSOR_CHAN_GAUGE_FULL_CHARGE_CAPACITY,
&full_charge_capacity);
if (status < 0) {
printk("Unable to get full charge capacity\n");
return;
}

printk("Full charge capacity: %d.%06dAh\n",
full_charge_capacity.val1, full_charge_capacity.val2);

status = sensor_sample_fetch_chan(bq,
SENSOR_CHAN_GAUGE_REMAINING_CHARGE_CAPACITY);
if (status < 0) {
printk("Unable to fetch Remaining Charge Capacity\n");
return;
}

status = sensor_channel_get(bq,
SENSOR_CHAN_GAUGE_REMAINING_CHARGE_CAPACITY,
&remaining_charge_capacity);
if (status < 0) {
printk("Unable to get remaining charge capacity\n");
return;
}

printk("Remaining charge capacity: %d.%06dAh\n",
remaining_charge_capacity.val1,
remaining_charge_capacity.val2);

status = sensor_sample_fetch_chan(bq, SENSOR_CHAN_GAUGE_TEMP);
if (status < 0) {
printk("Failed to fetch Gauge Temp\n");
return;
}

status = sensor_channel_get(bq, SENSOR_CHAN_GAUGE_TEMP,
      &int_temp);
if (status < 0) {
printk("Unable to read internal temperature\n");
return;
}

printk("Gauge Temperature: %d.%06d C\n", int_temp.val1,
int_temp.val2);
    


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

void batteryThreadinit(void) {
    k_thread_create(&batteryUpdateThread_data, batteryUpdateThread_stack_area,
                    K_THREAD_STACK_SIZEOF(batteryUpdateThread_stack_area),
                    batteryUpdateThread, NULL, NULL, NULL,
                    PRIORITY, 0, K_NO_WAIT);

}

SYS_INIT(batteryThreadinit, APPLICATION, 50);