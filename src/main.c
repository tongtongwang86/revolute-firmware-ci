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

LOG_MODULE_REGISTER(button_handler, LOG_LEVEL_DBG);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define SW0_NODE    DT_ALIAS(sw0)
#define SW1_NODE    DT_ALIAS(sw1)
#define SW2_NODE    DT_ALIAS(sw2)
#define SW3_NODE    DT_ALIAS(sw3)
#define LED0_NODE   DT_ALIAS(led0)

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




enum button_evt {
    BUTTON_EVT_PRESSED,
    BUTTON_EVT_RELEASED
};

typedef void (*button_event_handler_t)(size_t idx, enum button_evt evt);

static const struct gpio_dt_spec leds[] = {
    GPIO_DT_SPEC_GET_OR(LED0_NODE, gpios, {0}),
};

static const struct gpio_dt_spec buttons[] = {
    GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0}),
    GPIO_DT_SPEC_GET_OR(SW1_NODE, gpios, {0}),
    GPIO_DT_SPEC_GET_OR(SW2_NODE, gpios, {0}),
    GPIO_DT_SPEC_GET_OR(SW3_NODE, gpios, {0}),
};

static struct gpio_callback button_cb_data[ARRAY_SIZE(buttons)];
static button_event_handler_t user_cb[ARRAY_SIZE(buttons)];
static bool button_pressed[ARRAY_SIZE(buttons)] = {false};


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
	err = gpio_pin_toggle_dt(&leds[0]);
                if (err < 0) {
                    return err;
                }
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason %u)\n", reason);

	int err = gpio_pin_toggle_dt(&leds[0]);
                if (err < 0) {
                    return err;
                }
}

/* STEP 5.2 Define the callback function security_changed() */
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
	/* STEP 5.1 - Add the security_changed member to the callback structure */
	.security_changed = on_security_changed,
};

/* STEP 9.1 - Define the callback function auth_passkey_display */
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Passkey for %s: %06u\n", addr, passkey);
}

/* STEP 9.2 - Define the callback function auth_cancel */
static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing cancelled: %s\n", addr);
}

/* STEP 9.3 - Declare the authenticated pairing callback structure */
static struct bt_conn_auth_cb conn_auth_callbacks = {
	// .passkey_display = auth_passkey_display,
	.cancel = auth_cancel,
};


void button_pressed_handler(const struct device *dev, struct gpio_callback *cb,
                            uint32_t pins)
{
    size_t idx = 0;
    for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
        if (cb == &button_cb_data[i]) {
            idx = i;
            break;
        }
    }

    if (!button_pressed[idx]) {
        // Button pressed event
        button_pressed[idx] = true;
        if (user_cb[idx]) {
            user_cb[idx](idx, BUTTON_EVT_PRESSED);
        }
    } else {
        // Button released event
        button_pressed[idx] = false;
        if (user_cb[idx]) {
            user_cb[idx](idx, BUTTON_EVT_RELEASED);
        }
    }
}

int button_init(size_t idx, button_event_handler_t handler)
{
    if (idx >= ARRAY_SIZE(buttons)) {
        return -EINVAL;
    }

    if (!handler) {
        return -EINVAL;
    }

    user_cb[idx] = handler;

    if (!device_is_ready(buttons[idx].port)) {
        LOG_ERR("Button %zu port not ready", idx);
        return -EIO;
    }

    int err = gpio_pin_configure_dt(&buttons[idx], GPIO_INPUT);
    if (err) {
        LOG_ERR("Failed to configure button %zu: %d", idx, err);
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(&buttons[idx], GPIO_INT_EDGE_BOTH);
    if (err) {
        LOG_ERR("Failed to configure interrupt for button %zu: %d", idx, err);
        return err;
    }

    gpio_init_callback(&button_cb_data[idx], button_pressed_handler, BIT(buttons[idx].pin));
    err = gpio_add_callback(buttons[idx].port, &button_cb_data[idx]);
    if (err) {
        LOG_ERR("Failed to add callback for button %zu: %d", idx, err);
        return err;
    }

    LOG_INF("Button %zu initialized", idx);

    // Initialize LED GPIO as output (assuming LEDs are used)
    if (!device_is_ready(leds[0].port)) {
        LOG_ERR("LED port not ready");
        return -EIO;
    }

    err = gpio_pin_configure_dt(&leds[0], GPIO_OUTPUT_ACTIVE);
    if (err < 0) {
        LOG_ERR("Failed to configure LED: %d", err);
        return err;
    }

    return 0;
}

static void button_event_handler(size_t idx, enum button_evt evt)
{
    int err;
    switch (idx) {
        case 0:
            if (evt == BUTTON_EVT_PRESSED) {
                err = gpio_pin_toggle_dt(&leds[0]);
                if (err < 0) {
                    return err;
                }
                LOG_INF("Button 1 pressed");
                
            } else {
                err = gpio_pin_toggle_dt(&leds[0]);
                if (err < 0) {
                    return err;
                }
                LOG_INF("Button 1 released");
                
            }
            break;
        case 1:
            if (evt == BUTTON_EVT_PRESSED) {
                err = gpio_pin_toggle_dt(&leds[0]);
                if (err < 0) {
                    return err;
                }
                LOG_INF("Button 2 pressed");
            } else {
                LOG_INF("Button 2 released");
            }
            break;
        case 2:
            if (evt == BUTTON_EVT_PRESSED) {
                LOG_INF("Button 3 pressed");
            } else {
                err = gpio_pin_toggle_dt(&leds[0]);
                if (err < 0) {
                    return err;
                }
                LOG_INF("Button 3 released");
            }
            break;
        case 3:
            if (evt == BUTTON_EVT_PRESSED) {
                LOG_INF("Button 4 pressed");
            } else {
                LOG_INF("Button 4 released");
            }
            break;
        default:
            LOG_ERR("Unknown button %zu event", idx + 1);
            break;
    }
}

void main(void)
{
    log_init();
    LOG_INF("Button Debouncing Sample!");

    int err = bt_conn_auth_cb_register(&conn_auth_callbacks);
	if (err) {
		LOG_INF("Failed to register authorization callbacks\n");
		return -1;
	}

	bt_conn_cb_register(&connection_callbacks);

	err = bt_enable(NULL);
	if (err) {
		LOG_INF("Bluetooth init failed (err %d)\n", err);
		return -1;
	}

    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_INF("Advertising failed to start (err %d)\n", err);
		return -1;
	}

    LOG_INF("Advertising successfully started\n");

    // Initialize buttons
    for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
        int err = button_init(i, button_event_handler);
        if (err) {
            LOG_ERR("Button %zu Init failed: %d", i + 1, err);
            return;
        }
    }

    LOG_INF("Init succeeded. Waiting for event...");

    // Main loop
    while (1) {
        k_sleep(K_FOREVER);
    }
}