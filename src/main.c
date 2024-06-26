/* main.c - Application main entry point */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */




#include <zephyr/drivers/gpio.h>


#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

// #include "hog.h"

// logging stuff
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/types.h>


//magnetic angle sensor
#include <zephyr/drivers/sensor.h>



#define PRIORITY 1
#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)
#define SW2_NODE DT_ALIAS(sw2)
#define SW3_NODE DT_ALIAS(sw3)

#define STACKSIZE 1024
K_THREAD_STACK_DEFINE(batteryUpdateThread_stack_area, STACKSIZE);

static struct k_thread batteryUpdateThread_data;

struct sensor_value voltage, current, state_of_charge,
	full_charge_capacity, remaining_charge_capacity, avg_power,
	int_temp, current_standby, current_max_load, state_of_health;



void batteryUpdateThread()
{
	
  const struct device *const bq = DEVICE_DT_GET_ONE(ti_bq274xx);

	if (!device_is_ready(bq)) {
		printk("Device %s is not ready\n", bq->name);
		return 0;
	}

	printk("device is %p, name is %s\n", bq, bq->name);


	uint8_t level ;


while(1){

level = getbatterylevel(bq);

			printk("State of charge: %d%%\n", level);

	int err;
err = bt_bas_set_battery_level (level);
if (err) {
		// LOG_INF("cant send battery report", err);
		return 0;
	}

k_sleep(K_MSEC(1000));


}
}

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

int getbatterylevel(const struct device *dev)
{

	int status = 0;
    struct sensor_value state_of_charge;


	status = sensor_sample_fetch_chan(dev,
					SENSOR_CHAN_GAUGE_STATE_OF_CHARGE);
		if (status < 0) {
			printk("Unable to fetch State of Charge\n");
			return;
		}


	status = sensor_channel_get(dev,
					    SENSOR_CHAN_GAUGE_STATE_OF_CHARGE,
					    &state_of_charge);
		if (status < 0) {
			printk("Unable to get state of charge\n");
			return;
		}

	

    return state_of_charge.val1;
}




enum {
	HIDS_REMOTE_WAKE = BIT(0),
	HIDS_NORMALLY_CONNECTABLE = BIT(1),
};

struct hids_info {
	uint16_t version; /* version number of base USB HID Specification */
	uint8_t code; /* country HID Device hardware is localized for. */
	uint8_t flags;
} __packed;

struct hids_report {
	uint8_t id; /* report id */
	uint8_t type; /* report type */
} __packed;

static struct hids_info info = {
	.version = 0x0000,
	.code = 0x00,
	.flags = HIDS_NORMALLY_CONNECTABLE,
};

enum {
	HIDS_INPUT = 0x01,
	HIDS_OUTPUT = 0x02,
	HIDS_FEATURE = 0x03,
};

static struct hids_report input = {
	.id = 0x01,
	.type = HIDS_INPUT,
};

static uint8_t simulate_input;
static uint8_t ctrl_point;
static uint8_t report_map[] = {
	0x05, 0x01, /* Usage Page (Generic Desktop Ctrls) */
	0x09, 0x02, /* Usage (Mouse) */
	0xA1, 0x01, /* Collection (Application) */
	0x85, 0x01, /*	 Report Id (1) */
	0x09, 0x01, /*   Usage (Pointer) */
	0xA1, 0x00, /*   Collection (Physical) */
	0x05, 0x09, /*     Usage Page (Button) */
	0x19, 0x01, /*     Usage Minimum (0x01) */
	0x29, 0x03, /*     Usage Maximum (0x03) */
	0x15, 0x00, /*     Logical Minimum (0) */
	0x25, 0x01, /*     Logical Maximum (1) */
	0x95, 0x03, /*     Report Count (3) */
	0x75, 0x01, /*     Report Size (1) */
	0x81, 0x02, /*     Input (Data,Var,Abs,No Wrap,Linear,...) */
	0x95, 0x01, /*     Report Count (1) */
	0x75, 0x05, /*     Report Size (5) */
	0x81, 0x03, /*     Input (Const,Var,Abs,No Wrap,Linear,...) */
	0x05, 0x01, /*     Usage Page (Generic Desktop Ctrls) */
	0x09, 0x30, /*     Usage (X) */
	0x09, 0x31, /*     Usage (Y) */
	0x15, 0x81, /*     Logical Minimum (129) */
	0x25, 0x7F, /*     Logical Maximum (127) */
	0x75, 0x08, /*     Report Size (8) */
	0x95, 0x02, /*     Report Count (2) */
	0x81, 0x06, /*     Input (Data,Var,Rel,No Wrap,Linear,...) */
	0xC0,       /*   End Collection */
	0xC0,       /* End Collection */
};


static ssize_t read_info(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr, void *buf,
			  uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_info));
}

static ssize_t read_report_map(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, report_map,
				 sizeof(report_map));
}

static ssize_t read_report(struct bt_conn *conn,
			   const struct bt_gatt_attr *attr, void *buf,
			   uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_report));
}

static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	simulate_input = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static ssize_t read_input_report(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr, void *buf,
				 uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, NULL, 0);
}

static ssize_t write_ctrl_point(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
	uint8_t *value = attr->user_data;

	if (offset + len > sizeof(ctrl_point)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);

	return len;
}

#if CONFIG_SAMPLE_BT_USE_AUTHENTICATION
/* Require encryption using authenticated link-key. */
#define SAMPLE_BT_PERM_READ BT_GATT_PERM_READ_AUTHEN
#define SAMPLE_BT_PERM_WRITE BT_GATT_PERM_WRITE_AUTHEN
#else
/* Require encryption. */
#define SAMPLE_BT_PERM_READ BT_GATT_PERM_READ_ENCRYPT
#define SAMPLE_BT_PERM_WRITE BT_GATT_PERM_WRITE_ENCRYPT
#endif

BT_GATT_SERVICE_DEFINE(hog_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_info, NULL, &info),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_report_map, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       SAMPLE_BT_PERM_READ,
			       read_input_report, NULL, NULL),
	BT_GATT_CCC(input_ccc_changed,
		    SAMPLE_BT_PERM_READ | SAMPLE_BT_PERM_WRITE),
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
			   read_report, NULL, &input),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_WRITE,
			       NULL, write_ctrl_point, &ctrl_point),
);



LOG_MODULE_REGISTER(Rev,LOG_LEVEL_DBG);
BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
             "Console device is not ACM CDC UART device");



static unsigned char url_data[] = { 0x17, '/', '/', 't', 'o', 'n', 'g', 't', 'o',
				    'n',  'g', 'i', 'n', 'c', '.', 'c', 'o', 'm' };


static const struct bt_data ad[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_URI, url_data, sizeof(url_data)),
};






static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		LOG_INF("Failed to connect to %s (%u)\n", addr, err);
		return;
	}

	LOG_INF("Connected %s\n", addr);

	if (bt_conn_set_security(conn, BT_SECURITY_L2)) {
		LOG_INF("Failed to set security\n");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	int err;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected from %s (reason 0x%02x)\n", addr, reason);
	
	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_INF("Advertising failed to start (err %d)\n", err);
		return;
	}

}

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_INF("Security changed: %s level %u\n", addr, level);
	} else {
		LOG_INF("Security failed: %s level %u err %d\n", addr, level,
		       err);
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
};



static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.passkey_display = auth_passkey_display,
	.passkey_entry = NULL,
	.cancel = auth_cancel,
};

int main(void)
{




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



// 	while (!dtr) {
//     uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
//     /* Give CPU resources to low priority threads. */
//     k_sleep(K_MSEC(100));
//   }


  k_thread_create(&batteryUpdateThread_data, batteryUpdateThread_stack_area,
			K_THREAD_STACK_SIZEOF(batteryUpdateThread_stack_area),
			batteryUpdateThread, NULL, NULL, NULL,
			PRIORITY, 0, K_FOREVER);
	k_thread_name_set(&batteryUpdateThread_data, "batterythread");

	k_thread_start(&batteryUpdateThread_data);


  const struct device *const as = DEVICE_DT_GET(DT_INST(0,ams_as5600));

	if (as == NULL || !device_is_ready(as)) {
		printk("as5600 device tree not configured\n");
		return;
	}
	


	int err;

	err = bt_enable(NULL);
	if (err) {
		LOG_INF("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

			settings_load();

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_INF("Advertising failed to start (err %d)\n", err);
	}

			LOG_INF("Advertising successfully started\n");


	if (IS_ENABLED(CONFIG_SAMPLE_BT_USE_AUTHENTICATION)) {
		bt_conn_auth_cb_register(&auth_cb_display);
		LOG_INF("Bluetooth authentication callbacks registered.\n");
	}

	const struct gpio_dt_spec sw0 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
	const struct gpio_dt_spec sw1 = GPIO_DT_SPEC_GET(SW1_NODE, gpios);
	const struct gpio_dt_spec sw2 = GPIO_DT_SPEC_GET(SW2_NODE, gpios);
	const struct gpio_dt_spec sw3 = GPIO_DT_SPEC_GET(SW3_NODE, gpios);

	gpio_pin_configure_dt(&sw0, GPIO_INPUT);
	gpio_pin_configure_dt(&sw1, GPIO_INPUT);
	gpio_pin_configure_dt(&sw2, GPIO_INPUT);
	gpio_pin_configure_dt(&sw3, GPIO_INPUT);




LOG_INF("starting system");

while (1) {
			

			int degrees = as5600_refresh(as) ;
			int usefulDegrees = as5600_refresh(as) ;
			int lastDegree = as5600_refresh(as);
			int deltadegrees = 0;
			int lastDeltadegrees = 0;
			int deltaDeltadegrees = 0;
			int lastdeltaDeltadegrees = 0;
			deltadegrees = (degrees-lastDegree);
			deltaDeltadegrees = (deltadegrees-lastDeltadegrees);
			// printk ("%d\n",degrees);
			if (deltaDeltadegrees < -200){
			usefulDegrees = lastDeltadegrees ;
			deltaDeltadegrees = lastdeltaDeltadegrees;

			} else if(deltaDeltadegrees > 200) {
				usefulDegrees = lastDeltadegrees ;
				deltaDeltadegrees = lastdeltaDeltadegrees;

			}else{
				usefulDegrees = (degrees-lastDegree) ;

				// printk("%d\n",usefulDegrees);

			}


			int err_code;	


			if (gpio_pin_get_dt(&sw3)) {
			 err_code = bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
			if (err_code) {
				LOG_INF("Cannot delete bond (err: %d)\n", err);
			} else {
				LOG_INF("Bond deleted succesfully");
			}
			}


		if (simulate_input) {
			/* HID Report:
			 * Byte 0: buttons (lower 3 bits)
			 * Byte 1: X axis (int8)
			 * Byte 2: Y axis (int8)
			 */
			int8_t report[3] = {0, 0, 0};


if(degrees != lastDegree){
		if (deltaDeltadegrees > 0){
			// state += (deltaDeltadegrees);
			report[1] = 10;
			// LOG_INF("right");

		}else{
			// state += (deltaDeltadegrees);
			report[1] = -10;
			// LOG_INF("left");
		}
		}

		lastDegree = degrees;
		lastDeltadegrees = deltadegrees;
		lastdeltaDeltadegrees = deltaDeltadegrees;


	if (gpio_pin_get_dt(&sw1)) {
				report[1] = -10;
				LOG_INF("right");
			}

			if (gpio_pin_get_dt(&sw0)) {
				report[1] = 10;
				LOG_INF("right");
			}
		
			if (gpio_pin_get_dt(&sw2)) {
				report[0] |= BIT(0);
				LOG_INF("left click");
			}


			bt_gatt_notify(NULL, &hog_svc.attrs[5],
				       report, sizeof(report));
		}
		k_sleep(K_MSEC(5));
  }
	

	return 0;
}


