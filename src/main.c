






#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>


#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(button_handler, LOG_LEVEL_DBG);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)


#define BT_LE_ADV_CONN_NO_ACCEPT_LIST                                                              \
	BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_ONE_TIME,                        \
			BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL)
/* STEP 3.2.2 - Define advertising parameter for when Accept List is used */
#define BT_LE_ADV_CONN_ACCEPT_LIST                                                                 \
	BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_FILTER_CONN |                    \
				BT_LE_ADV_OPT_ONE_TIME,                                            \
			BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL)

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
	// BT_DATA_BYTES(BT_DATA_UUID128_ALL, LOGGER_BACKEND_BLE_ADV_UUID_DATA)
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};





#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>


#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/settings/settings.h>


#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define SW0_NODE    DT_ALIAS(sw0)
#define LED0_NODE   DT_ALIAS(led0)

enum button_evt {
    BUTTON_EVT_PRESSED,
    BUTTON_EVT_RELEASED
};

typedef void (*button_event_handler_t)(enum button_evt evt);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);

static struct gpio_callback button_cb_data;

static button_event_handler_t user_cb;

static void cooldown_expired(struct k_work *work)
{
    ARG_UNUSED(work);

    int val = gpio_pin_get_dt(&button);
    enum button_evt evt = val ? BUTTON_EVT_PRESSED : BUTTON_EVT_RELEASED;
    if (user_cb) {
        user_cb(evt);
    }
}

static K_WORK_DELAYABLE_DEFINE(cooldown_work, cooldown_expired);

void button_pressed(const struct device *dev, struct gpio_callback *cb,
                    uint32_t pins)
{
    k_work_reschedule(&cooldown_work, K_MSEC(15));
}

int button_init(button_event_handler_t handler)
{
    if (!handler) {
        return -EINVAL;
    }

    user_cb = handler;

    if (!device_is_ready(button.port)) {
        return -EIO;
    }

    int err = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (err) {
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
    if (err) {
        return err;
    }

    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    err = gpio_add_callback(button.port, &button_cb_data);
    if (err) {
        return err;
    }

    return 0;
}

static char *helper_button_evt_str(enum button_evt evt)
{
    int err;
    switch (evt) {
    case BUTTON_EVT_PRESSED:
        err = gpio_pin_toggle_dt(&led);
        if (err < 0) {
            return "Error";
        }
         err = bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
			if (err) {
				LOG_INF("Cannot delete bond (err: %d)\n", err);
			} else {
				LOG_INF("Bond deleted succesfully");
			}

        return "Pressed";
    case BUTTON_EVT_RELEASED:
        err = gpio_pin_toggle_dt(&led);
        if (err < 0) {
            return "Error";
        }
        return "Released";
    default:
        return "Unknown";
    }
}

static void button_event_handler(enum button_evt evt)
{
    printk("Button event: %s\n", helper_button_evt_str(evt));
}







static void on_connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_INF("Connection failed (err %u)\n", err);
		return;
	}

	LOG_INF("Connected\n");
    if (bt_conn_set_security(conn, BT_SECURITY_L2)) {
		LOG_INF("Failed to set security\n");
	}
	err = gpio_pin_toggle_dt(&led);
        if (err < 0) {
            return "Error";
        }
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason %u)\n", reason);

	int err = gpio_pin_toggle_dt(&led);
        if (err < 0) {
            return "Error";
        }
	/* STEP 3.5 - Start advertising with Accept List */
	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
				 sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_INF("Advertising failed to start (err %d)\n", err);
		return;
	}
    
}

static void on_security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_INF("Security changed: %s level %u\n", addr, level);
	} else {
		LOG_INF("Security failed: %s level %u err %d\n", addr, level, err);
	}
}
struct bt_conn_cb connection_callbacks = {
	.connected = on_connected,
	.disconnected = on_disconnected,
	.security_changed = on_security_changed,
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

static struct bt_conn_auth_cb conn_auth_callbacks = {
	// .passkey_display = auth_passkey_display,
	.cancel = auth_cancel,
};





void main(void)
{
    log_init();
    LOG_INF("Button Debouncing Sample!");
    
    int err = bt_conn_auth_cb_register(&conn_auth_callbacks);
	if (err) {
		LOG_INF("Failed to register authorization callbacks.\n");
		return -1;
	}
 

    bt_conn_cb_register(&connection_callbacks);
    

    LOG_INF("Initializing settings subsystem...");
    err = settings_subsys_init();
    if (err) {
        LOG_ERR("Settings subsystem initialization failed (err %d)", err);
        return;
    }

    // settings_subsys_init();



	err = bt_enable(NULL);
	if (err) {
		LOG_INF("Bluetooth init failed (err %d)\n", err);
		return -1;
	}
    LOG_INF("Bluetooth initialized\n");
    err = settings_load();
    if (err) {
		LOG_INF("settings gay (err %d)\n", err);
		return -1;
	}
     LOG_INF("settings loaded\n");

    
      
   
	/* STEP 1.3 - Add setting load function */
	
    // k_work_submit(&advertise_acceptlist_work);
    	/* STEP 3.4.3 - Remove the original code that does normal advertising */
	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
				 sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_INF("Advertising failed to start (err %d)\n", err);
		return;
	}
	LOG_INF("Advertising successfully started\n");

    err = button_init(button_event_handler);
    if (err) {
        printk("Button Init failed: %d\n", err);
        return err;
    }

    if (!device_is_ready(led.port)) {
        return -EIO;
    }

    err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (err < 0) {
        return err;
    }

    LOG_INF("Init succeeded. Waiting for event...");

    // Main loop
    for (;;) {
        k_sleep(K_FOREVER);
	}
}