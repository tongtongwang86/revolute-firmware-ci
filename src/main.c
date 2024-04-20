/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>




#define SW0_NODE	DT_ALIAS(sw0) 
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);


#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);



LOG_MODULE_REGISTER(Rev,LOG_LEVEL_DBG);


BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
             "Console device is not ACM CDC UART device");

int as5600_refresh(const struct device *dev)
{
	int ret;
    struct sensor_value rot_raw;
    ret = sensor_sample_fetch_chan(dev,SENSOR_CHAN_ROTATION);
	if (ret != 0){
			printk("sample fetch error:,%d\n", ret);
		}
    sensor_channel_get(dev,SENSOR_CHAN_ROTATION, &rot_raw);
	

    return rot_raw.val1;
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


void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    gpio_pin_toggle_dt(&led);
    LOG_INF("toggled led");
    LOG_WRN("wrn led");
}

static struct gpio_callback button_cb_data;

int main(void) {

  const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
  

  uint32_t dtr = 0;

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
  if (enable_usb_device_next()) {
    return 0;
  }
#else
  if (usb_enable(NULL)) {
    return 0;
  }
#endif


  /* Poll if the DTR flag was set */
  while (!dtr) {
    uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
    /* Give CPU resources to low priority threads. */
    k_sleep(K_MSEC(100));
  }

//   const struct device *const bq27 = DEVICE_DT_GET_ONE(ti_bq274xx);
  // const struct device *const bq27 = DEVICE_DT_GET(DT_INST(0,ti_bq274xx));
  const struct device *const as = DEVICE_DT_GET_ONE(ams_as5600); 

//   if (!device_is_ready(bq27)) {
// 		printk("Device %s is not ready\n", bq27->name);
// 		return 0;
// 	}

  if (as == NULL || !device_is_ready(as)) {
		printk("as5600 device tree not configured\n");
		return;
	}



  if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

  if (!device_is_ready(button.port)) {
	return -1;
}

int ret;

ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
if (ret < 0) {
    return;
}

ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret < 0) {
		return -1;
	}


  ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);

if (ret < 0) {
  LOG_ERR("set pin as interrupt failed");
	return -1;
}




    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin)); 	
	gpio_add_callback(button.port, &button_cb_data);


// printk("device is %p, name is %s\n", bq27, bq27->name);

// int status = 0;
// 	struct sensor_value voltage, current, state_of_charge,
// 		full_charge_capacity, remaining_charge_capacity, avg_power,
// 		int_temp, current_standby, current_max_load, state_of_health;


  while (1) {
    // bool val = gpio_pin_get_dt(&button);
    // gpio_pin_set_dt(&led,val);
    printk("Hello World! %s\n", CONFIG_ARCH);
    LOG_INF("A log message in info level");
    LOG_DBG("A log message in debug level");
    LOG_WRN("A log message in warning level!");
    LOG_ERR("A log message in Error level!");


    // status = sensor_sample_fetch_chan(bq27,
		// 				  SENSOR_CHAN_GAUGE_VOLTAGE);
		// if (status < 0) {
		// 	printk("Unable to fetch the voltage\n");
		// 	return;
		// }

		// status = sensor_channel_get(bq27, SENSOR_CHAN_GAUGE_VOLTAGE,
		// 			    &voltage);
		// if (status < 0) {
		// 	printk("Unable to get the voltage value\n");
		// 	return;
		// }

		// printk("Voltage: %d.%06dV\n", voltage.val1, voltage.val2);

		// status = sensor_sample_fetch_chan(bq27,
		// 			       SENSOR_CHAN_GAUGE_AVG_CURRENT);
		// if (status < 0) {
		// 	printk("Unable to fetch the Average current\n");
		// 	return;
		// }

		// status = sensor_channel_get(bq27, SENSOR_CHAN_GAUGE_AVG_CURRENT,
		// 			    &current);
		// if (status < 0) {
		// 	printk("Unable to get the current value\n");
		// 	return;
		// }

		// bq274xx_show_values("Avg Current in Amps", current);

		// status = sensor_sample_fetch_chan(bq27,
		// 			SENSOR_CHAN_GAUGE_STDBY_CURRENT);
		// if (status < 0) {
		// 	printk("Unable to fetch Standby Current\n");
		// 	return;
		// }

		// status = sensor_channel_get(bq27,
		// 			SENSOR_CHAN_GAUGE_STDBY_CURRENT,
		// 			&current_standby);
		// if (status < 0) {
		// 	printk("Unable to get the current value\n");
		// 	return;
		// }

		// bq274xx_show_values("Standby Current in Amps", current_standby);

		// status = sensor_sample_fetch_chan(bq27,
		// 			SENSOR_CHAN_GAUGE_MAX_LOAD_CURRENT);
		// if (status < 0) {
		// 	printk("Unable to fetch Max Load Current\n");
		// 	return;
		// }

		// status = sensor_channel_get(bq27,
		// 			SENSOR_CHAN_GAUGE_MAX_LOAD_CURRENT,
		// 			&current_max_load);
		// if (status < 0) {
		// 	printk("Unable to get the current value\n");
		// 	return;
		// }

		// bq274xx_show_values("Max Load Current in Amps",
		// 		    current_max_load);

		// status = sensor_sample_fetch_chan(bq27,
		// 			SENSOR_CHAN_GAUGE_STATE_OF_CHARGE);
		// if (status < 0) {
		// 	printk("Unable to fetch State of Charge\n");
		// 	return;
		// }

		// status = sensor_channel_get(bq27,
		// 			    SENSOR_CHAN_GAUGE_STATE_OF_CHARGE,
		// 			    &state_of_charge);
		// if (status < 0) {
		// 	printk("Unable to get state of charge\n");
		// 	return;
		// }

		// printk("State of charge: %d%%\n", state_of_charge.val1);

		// status = sensor_sample_fetch_chan(bq27,
		// 			SENSOR_CHAN_GAUGE_STATE_OF_HEALTH);
		// if (status < 0) {
		// 	printk("Failed to fetch State of Health\n");
		// 	return;
		// }

		// status = sensor_channel_get(bq27,
		// 			    SENSOR_CHAN_GAUGE_STATE_OF_HEALTH,
		// 			    &state_of_health);
		// if (status < 0) {
		// 	printk("Unable to get state of charge\n");
		// 	return;
		// }

		// printk("State of health: %d%%\n", state_of_health.val1);

		// status = sensor_sample_fetch_chan(bq27,
		// 			SENSOR_CHAN_GAUGE_AVG_POWER);
		// if (status < 0) {
		// 	printk("Unable to fetch Avg Power\n");
		// 	return;
		// }

		// status = sensor_channel_get(bq27, SENSOR_CHAN_GAUGE_AVG_POWER,
		// 			    &avg_power);
		// if (status < 0) {
		// 	printk("Unable to get avg power\n");
		// 	return;
		// }

		// bq274xx_show_values("Avg Power in Watt", avg_power);

		// status = sensor_sample_fetch_chan(bq27,
		// 		SENSOR_CHAN_GAUGE_FULL_CHARGE_CAPACITY);
		// if (status < 0) {
		// 	printk("Failed to fetch Full Charge Capacity\n");
		// 	return;
		// }

		// status = sensor_channel_get(bq27,
		// 		SENSOR_CHAN_GAUGE_FULL_CHARGE_CAPACITY,
		// 		&full_charge_capacity);
		// if (status < 0) {
		// 	printk("Unable to get full charge capacity\n");
		// 	return;
		// }

		// printk("Full charge capacity: %d.%06dAh\n",
		//        full_charge_capacity.val1, full_charge_capacity.val2);

		// status = sensor_sample_fetch_chan(bq27,
		// 		SENSOR_CHAN_GAUGE_REMAINING_CHARGE_CAPACITY);
		// if (status < 0) {
		// 	printk("Unable to fetch Remaining Charge Capacity\n");
		// 	return;
		// }

		// status = sensor_channel_get(bq27,
		// 		SENSOR_CHAN_GAUGE_REMAINING_CHARGE_CAPACITY,
		// 		&remaining_charge_capacity);
		// if (status < 0) {
		// 	printk("Unable to get remaining charge capacity\n");
		// 	return;
		// }

		// printk("Remaining charge capacity: %d.%06dAh\n",
		//        remaining_charge_capacity.val1,
		//        remaining_charge_capacity.val2);

		// status = sensor_sample_fetch_chan(bq27, SENSOR_CHAN_GAUGE_TEMP);
		// if (status < 0) {
		// 	printk("Failed to fetch Gauge Temp\n");
		// 	return;
		// }

		// status = sensor_channel_get(bq27, SENSOR_CHAN_GAUGE_TEMP,
		// 			    &int_temp);
		// if (status < 0) {
		// 	printk("Unable to read internal temperature\n");
		// 	return;
		// }

		// printk("Gauge Temperature: %d.%06d C\n", int_temp.val1,
		//        int_temp.val2);
    // ret = gpio_pin_toggle_dt(&led);
    //  if (ret < 0) {
    //     return;
    // }
    printk("degrees:%d\n",as5600_refresh(as));

    k_sleep(K_MSEC(1000));
  }
}
