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
};
static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME)};





#define SW0_NODE    DT_ALIAS(sw0)
#define SW1_NODE    DT_ALIAS(sw1)
#define SW2_NODE    DT_ALIAS(sw2)
#define SW3_NODE    DT_ALIAS(sw3)
#define LED0_NODE   DT_ALIAS(led0)

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


static void setup_accept_list_cb(const struct bt_bond_info *info, void *user_data)
{
	int *bond_cnt = user_data;

	if ((*bond_cnt) < 0) {
		return;
	}

	int err = bt_le_filter_accept_list_add(&info->addr);
	LOG_INF("Added following peer to accept list: %x %x\n", info->addr.a.val[0],
		info->addr.a.val[1]);
	if (err) {
		LOG_INF("Cannot add peer to filter accept list (err: %d)\n", err);
		(*bond_cnt) = -EIO;
	} else {
		(*bond_cnt)++;
	}
}

/* STEP 3.3.2 - Define the function to loop through the bond list */
static int setup_accept_list(uint8_t local_id)
{
	int err = bt_le_filter_accept_list_clear();

	if (err) {
		LOG_INF("Cannot clear accept list (err: %d)\n", err);
		return err;
	}

	int bond_cnt = 0;

	bt_foreach_bond(local_id, setup_accept_list_cb, &bond_cnt);

	return bond_cnt;
}

/* STEP 3.4.1 - Define the function to advertise with the Accept List */
void advertise_with_acceptlist(struct k_work *work)
{
	int err = 0;
	int allowed_cnt = setup_accept_list(BT_ID_DEFAULT);
	if (allowed_cnt < 0) {
		LOG_INF("Acceptlist setup failed (err:%d)\n", allowed_cnt);
	} else {
		if (allowed_cnt == 0) {
			LOG_INF("Advertising with no Accept list \n");
			err = bt_le_adv_start(BT_LE_ADV_CONN_NO_ACCEPT_LIST, ad, ARRAY_SIZE(ad), sd,
					      ARRAY_SIZE(sd));
		} else {
			LOG_INF("Acceptlist setup number  = %d \n", allowed_cnt);
			err = bt_le_adv_start(BT_LE_ADV_CONN_ACCEPT_LIST, ad, ARRAY_SIZE(ad), sd,
					      ARRAY_SIZE(sd));
		}
		if (err) {
			LOG_INF("Advertising failed to start (err %d)\n", err);
			return;
		}
		LOG_INF("Advertising successfully started\n");
	}
}
K_WORK_DEFINE(advertise_acceptlist_work, advertise_with_acceptlist);

static void on_connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_INF("Connection failed (err %u)\n", err);
		return;
	}

	LOG_INF("Connected\n");

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
	/* STEP 3.5 - Start advertising with Accept List */
	k_work_submit(&advertise_acceptlist_work);
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
	.passkey_display = auth_passkey_display,
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
                int err = bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
			if (err) {
				LOG_INF("Cannot delete bond (err: %d)\n", err);
			} else {
				LOG_INF("Bond deleted succesfully");
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
                int err_code = bt_le_adv_stop();
			if (err_code) {
				LOG_INF("Cannot stop advertising err= %d \n", err_code);
				return;
			}
			err_code = bt_le_filter_accept_list_clear();
			if (err_code) {
				LOG_INF("Cannot clear accept list (err: %d)\n", err_code);
			} else {
				LOG_INF("Accept list cleared succesfully");
			}
			err_code = bt_le_adv_start(BT_LE_ADV_CONN_NO_ACCEPT_LIST, ad,
						   ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

			if (err_code) {
				LOG_INF("Cannot start open advertising (err: %d)\n", err_code);
			} else {
				LOG_INF("Advertising in pairing mode started");
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
		LOG_INF("Failed to register authorization callbacks.\n");
		return -1;
	}

    bt_conn_cb_register(&connection_callbacks);
    
	err = bt_enable(NULL);
	if (err) {
		LOG_INF("Bluetooth init failed (err %d)\n", err);
		return -1;
	}

    err = settings_load();
    if (err) {
		LOG_INF("settings gay (err %d)\n", err);
		return -1;
	}

    LOG_INF("Bluetooth initialized\n");
    
    LOG_INF("settings loaded\n");
	/* STEP 1.3 - Add setting load function */
	
    k_work_submit(&advertise_acceptlist_work);


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