#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

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
#include <zephyr/usb/class/usb_hid.h>

#include <hid_usage.h>
#include <hid_usage_pages.h>
#include <bluetooth/services/hids.h>


// magnetic angle sensor
#include <zephyr/drivers/sensor.h>

LOG_MODULE_REGISTER(button_handler, LOG_LEVEL_DBG);

#define STACKSIZE 1024
K_THREAD_STACK_DEFINE(batteryUpdateThread_stack_area, STACKSIZE);

#define PRIORITY 1
#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)
#define SW2_NODE DT_ALIAS(sw2)
#define SW3_NODE DT_ALIAS(sw3)
#define LED0_NODE DT_ALIAS(led0)

#define REPORT_ID_KEYBOARD 1
#define REPORT_ID_CONSUMER 2
#define REPORT_ID_MOUSE    3


#define ZMK_HID_MAIN_VAL_DATA (0x00 << 0)
#define ZMK_HID_MAIN_VAL_CONST (0x01 << 0)

#define ZMK_HID_MAIN_VAL_ARRAY (0x00 << 1)
#define ZMK_HID_MAIN_VAL_VAR (0x01 << 1)

#define ZMK_HID_MAIN_VAL_ABS (0x00 << 2)
#define ZMK_HID_MAIN_VAL_REL (0x01 << 2)

#define ZMK_HID_MAIN_VAL_NO_WRAP (0x00 << 3)
#define ZMK_HID_MAIN_VAL_WRAP (0x01 << 3)

#define ZMK_HID_MAIN_VAL_LIN (0x00 << 4)
#define ZMK_HID_MAIN_VAL_NON_LIN (0x01 << 4)

#define ZMK_HID_MAIN_VAL_PREFERRED (0x00 << 5)
#define ZMK_HID_MAIN_VAL_NO_PREFERRED (0x01 << 5)

#define ZMK_HID_MAIN_VAL_NO_NULL (0x00 << 6)
#define ZMK_HID_MAIN_VAL_NULL (0x01 << 6)

#define ZMK_HID_MAIN_VAL_NON_VOL (0x00 << 7)
#define ZMK_HID_MAIN_VAL_VOL (0x01 << 7)

#define ZMK_HID_MAIN_VAL_BIT_FIELD (0x00 << 8)
#define ZMK_HID_MAIN_VAL_BUFFERED_BYTES (0x01 << 8)
#define ZMK_HID_REPORT_ID_KEYBOARD 0x01
#define ZMK_HID_REPORT_ID_LEDS 0x01
#define ZMK_HID_REPORT_ID_CONSUMER 0x02
#define ZMK_HID_REPORT_ID_MOUSE 0x03


enum button_evt
{
    BUTTON_EVT_PRESSED,
    BUTTON_EVT_RELEASED
};

struct button_states
{
    bool button0;
    bool button1;
    bool button2;
    bool button3;
};

struct button_states button_s;

static struct k_thread batteryUpdateThread_data;

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

struct work_item
{
    struct k_work_delayable work;
    size_t idx;
};

static struct work_item work_items[ARRAY_SIZE(buttons)];

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

double as5600_subdegree_refresh(const struct device *dev)
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

    double degrees = rot_raw.val1 + (float)rot_raw.val2 / 1000;

    return degrees;
}

enum
{
    HIDS_REMOTE_WAKE = BIT(0),
    HIDS_NORMALLY_CONNECTABLE = BIT(1),
};

struct hids_info
{
    uint16_t version; /* version number of base USB HID Specification */
    uint8_t code;     /* country HID Device hardware is localized for. */
    uint8_t flags;
} __packed;

struct hids_report
{
    uint8_t id;   /* report id */
    uint8_t type; /* report type */
} __packed;

// static struct hids_info info = {
//     .version = 0x0000,
//     .code = 0x00,
//     .flags = HIDS_NORMALLY_CONNECTABLE,
// };

enum
{
    HIDS_INPUT = 0x01,
    HIDS_OUTPUT = 0x02,
    HIDS_FEATURE = 0x03,
};

// static struct hids_report input = {
//     .id = 0x01,
//     .type = HIDS_INPUT,
// };

static uint8_t simulate_input;
static uint8_t ctrl_point;

static const uint8_t zmk_hid_report_desc[] = {
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
    HID_USAGE(HID_USAGE_GD_KEYBOARD),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
    HID_REPORT_ID(ZMK_HID_REPORT_ID_KEYBOARD),
    HID_USAGE_PAGE(HID_USAGE_KEY),
    HID_USAGE_MIN8(HID_USAGE_KEY_KEYBOARD_LEFTCONTROL),
    HID_USAGE_MAX8(HID_USAGE_KEY_KEYBOARD_RIGHT_GUI),
    HID_LOGICAL_MIN8(0x00),
    HID_LOGICAL_MAX8(0x01),
    HID_REPORT_SIZE(0x01),
    HID_REPORT_COUNT(0x08),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),
    HID_USAGE_PAGE(HID_USAGE_KEY),
    HID_REPORT_SIZE(0x08),
    HID_REPORT_COUNT(0x01),
    HID_INPUT(ZMK_HID_MAIN_VAL_CONST | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),
    HID_USAGE_PAGE(HID_USAGE_KEY),
    HID_LOGICAL_MIN8(0x00),
    HID_LOGICAL_MAX16(0xFF, 0x00),
    HID_USAGE_MIN8(0x00),
    HID_USAGE_MAX8(0xFF),
    HID_REPORT_SIZE(0x08),
    HID_REPORT_COUNT(0x06),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_ARRAY | ZMK_HID_MAIN_VAL_ABS),
    HID_END_COLLECTION,

    HID_USAGE_PAGE(HID_USAGE_CONSUMER),
    HID_USAGE(HID_USAGE_CONSUMER_CONSUMER_CONTROL),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
    HID_REPORT_ID(ZMK_HID_REPORT_ID_CONSUMER),
    HID_USAGE_PAGE(HID_USAGE_CONSUMER),
    HID_LOGICAL_MIN8(0x00),
    HID_LOGICAL_MAX16(0xFF, 0x0F),
    HID_USAGE_MIN8(0x00),
    HID_USAGE_MAX16(0xFF, 0x0F),
    HID_REPORT_SIZE(0x10),
    HID_REPORT_COUNT(0x06),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_ARRAY | ZMK_HID_MAIN_VAL_ABS),
    HID_END_COLLECTION,


    HID_USAGE_PAGE(HID_USAGE_GD),
    HID_USAGE(HID_USAGE_GD_MOUSE),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
    HID_REPORT_ID(ZMK_HID_REPORT_ID_MOUSE),
    HID_USAGE(HID_USAGE_GD_POINTER),
    HID_COLLECTION(HID_COLLECTION_PHYSICAL),
    HID_USAGE_PAGE(HID_USAGE_BUTTON),
    HID_USAGE_MIN8(0x1),
    HID_USAGE_MAX8(0x05),
    HID_LOGICAL_MIN8(0x00),
    HID_LOGICAL_MAX8(0x01),
    HID_REPORT_SIZE(0x01),
    HID_REPORT_COUNT(0x5),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),
    // Constant padding for the last 3 bits.
    HID_REPORT_SIZE(0x03),
    HID_REPORT_COUNT(0x01),
    HID_INPUT(ZMK_HID_MAIN_VAL_CONST | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),
    // Some OSes ignore pointer devices without X/Y data.
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
    HID_USAGE(HID_USAGE_GD_X),
    HID_USAGE(HID_USAGE_GD_Y),
    HID_USAGE(HID_USAGE_GD_WHEEL),
    HID_LOGICAL_MIN8(-0x7F),
    HID_LOGICAL_MAX8(0x7F),
    HID_REPORT_SIZE(0x08),
    HID_REPORT_COUNT(0x03),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_REL),
    HID_END_COLLECTION,
    HID_END_COLLECTION,

};



static const struct hids_report input = {
    .id = REPORT_ID_KEYBOARD,
    .type = BT_HIDS_REPORT_TYPE_INPUT,
};

static const struct hids_report consumer_input = {
    .id = REPORT_ID_CONSUMER,
    .type = BT_HIDS_REPORT_TYPE_INPUT,
};

static const struct hids_report mouse_input = {
    .id = REPORT_ID_MOUSE,
    .type = BT_HIDS_REPORT_TYPE_INPUT,
};

#define INPUT_REPORT_SIZE 8

static struct bt_hids_info info = {
    .bcd_hid = 0x0111, /* HID Class Spec 1.11 */
    .b_country_code = 0x00, /* Hardware target country */
    .flags = BT_HIDS_REMOTE_WAKE | BT_HIDS_NORMALLY_CONNECTABLE,
};

static const uint8_t input_report[] = { /* Example input report */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static ssize_t read_hids_info(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data, sizeof(info));
}

static ssize_t read_hids_report_map(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, zmk_hid_report_desc, sizeof(zmk_hid_report_desc));
}

static ssize_t read_hids_input_report(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, input_report, sizeof(input_report));
}

static ssize_t read_hids_consumer_input_report(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, input_report, sizeof(input_report));
}

static ssize_t read_hids_mouse_input_report(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, input_report, sizeof(input_report));
}

// static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
// {
//     bool notif_enabled = (value == BT_GATT_CCC_NOTIFY);
//     printk("Notifications %s\n", notif_enabled ? "enabled" : "disabled");
// }

static ssize_t write_ctrl_point(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    printk("Control point written\n");
    return len;
}

static ssize_t read_hids_report_ref(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                    void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
                             sizeof(struct hids_report));
}

// static ssize_t read_info(struct bt_conn *conn,
//                          const struct bt_gatt_attr *attr, void *buf,
//                          uint16_t len, uint16_t offset)
// {
// return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
//                              sizeof(struct hids_info));
// }

// static ssize_t read_report_map(struct bt_conn *conn,
//                                const struct bt_gatt_attr *attr, void *buf,
//                                uint16_t len, uint16_t offset)
// {
// return bt_gatt_attr_read(conn, attr, buf, len, offset, report_map,
//                              sizeof(report_map));
// }

// static ssize_t read_report(struct bt_conn *conn,
//                            const struct bt_gatt_attr *attr, void *buf,
//                            uint16_t len, uint16_t offset)
// {
//     return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
//                              sizeof(struct hids_report));
// }

static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    simulate_input = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

// static ssize_t read_input_report(struct bt_conn *conn,
//                                  const struct bt_gatt_attr *attr, void *buf,
//                                  uint16_t len, uint16_t offset)
// {
//     return bt_gatt_attr_read(conn, attr, buf, len, offset, NULL, 0);
// }

// static ssize_t write_ctrl_point(struct bt_conn *conn,
//                                 const struct bt_gatt_attr *attr,
//                                 const void *buf, uint16_t len, uint16_t offset,
//                                 uint8_t flags)
// {
//     uint8_t *value = attr->user_data;

//     if (offset + len > sizeof(ctrl_point))
//     {
//         return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
//     }

//     memcpy(value + offset, buf, len);

//     return len;
// }

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
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, read_hids_info, NULL, &info),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ, BT_GATT_PERM_READ_ENCRYPT, read_hids_report_map, NULL, NULL),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_ENCRYPT, read_hids_input_report, NULL, NULL),
    BT_GATT_CCC(input_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ_ENCRYPT, read_hids_report_ref, NULL, &input),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_ENCRYPT, read_hids_consumer_input_report, NULL, NULL),
    BT_GATT_CCC(input_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ_ENCRYPT, read_hids_report_ref, NULL, &consumer_input),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_ENCRYPT, read_hids_mouse_input_report, NULL, NULL),
    BT_GATT_CCC(input_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ_ENCRYPT, read_hids_report_ref, NULL, &mouse_input),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT, BT_GATT_CHRC_WRITE_WITHOUT_RESP, BT_GATT_PERM_WRITE, NULL, write_ctrl_point, &ctrl_point)
);

static unsigned char url_data[] = {0x17, '/', '/', 't', 'o', 'n', 'g', 't', 'o',
                                   'n', 'g', 'i', 'n', 'c', '.', 'c', 'o', 'm'};

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

    if (err)
    {
        LOG_INF("Failed to connect to %s (%u)\n", addr, err);
        return;
    }

    LOG_INF("Connected %s\n", addr);

    if (bt_conn_set_security(conn, BT_SECURITY_L2))
    {
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
    if (err)
    {
        LOG_INF("Advertising failed to start (err %d)\n", err);
        return;
    }
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (!err)
    {
        LOG_INF("Security changed: %s level %u\n", addr, level);
    }
    else
    {
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

static void cooldown_expired(struct k_work *work)
{
    struct work_item *item = CONTAINER_OF(work, struct work_item, work.work);
    size_t idx = item->idx;
    int val = gpio_pin_get_dt(&buttons[idx]);
    enum button_evt evt = val ? BUTTON_EVT_PRESSED : BUTTON_EVT_RELEASED;

    if (user_cb[idx])
    {
        user_cb[idx](idx, evt);
    }
}

void button_pressed_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    size_t idx = 0;
    for (size_t i = 0; i < ARRAY_SIZE(buttons); i++)
    {
        if (cb == &button_cb_data[i])
        {
            idx = i;
            break;
        }
    }

    k_work_reschedule(&work_items[idx].work, K_MSEC(15));
}

int button_init(size_t idx, button_event_handler_t handler)
{
    if (idx >= ARRAY_SIZE(buttons))
    {
        return -EINVAL;
    }

    if (!handler)
    {
        return -EINVAL;
    }

    user_cb[idx] = handler;

    if (!device_is_ready(buttons[idx].port))
    {
        LOG_ERR("Button %zu port not ready", idx);
        return -EIO;
    }

    int err = gpio_pin_configure_dt(&buttons[idx], GPIO_INPUT);
    if (err)
    {
        LOG_ERR("Failed to configure button %zu: %d", idx, err);
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(&buttons[idx], GPIO_INT_EDGE_BOTH);
    if (err)
    {
        LOG_ERR("Failed to configure interrupt for button %zu: %d", idx, err);
        return err;
    }

    gpio_init_callback(&button_cb_data[idx], button_pressed_handler, BIT(buttons[idx].pin));
    err = gpio_add_callback(buttons[idx].port, &button_cb_data[idx]);
    if (err)
    {
        LOG_ERR("Failed to add callback for button %zu: %d", idx, err);
        return err;
    }

    LOG_INF("Button %zu initialized", idx);

    // Initialize work items
    work_items[idx].idx = idx;
    k_work_init_delayable(&work_items[idx].work, cooldown_expired);

    // Initialize LED GPIO as output (assuming LEDs are used)
    if (!device_is_ready(leds[0].port))
    {
        LOG_ERR("LED port not ready");
        return -EIO;
    }

    err = gpio_pin_configure_dt(&leds[0], GPIO_OUTPUT_ACTIVE);
    if (err < 0)
    {
        LOG_ERR("Failed to configure LED: %d", err);
        return err;
    }

    return 0;
}

static void button_event_handler(size_t idx, enum button_evt evt)
{
    int err;
    switch (idx)
    {
    case 0:
        if (evt == BUTTON_EVT_PRESSED)
        {
            err = gpio_pin_toggle_dt(&leds[0]);
            if (err < 0)
            {
                return;
            }
            LOG_INF("Button 0 pressed");
            button_s.button0 = true;
        }
        else
        {
            err = gpio_pin_toggle_dt(&leds[0]);
            if (err < 0)
            {
                return;
            }
            LOG_INF("Button 0 released");
            button_s.button0 = false;
        }
        break;
    case 1:
        if (evt == BUTTON_EVT_PRESSED)
        {
            err = gpio_pin_toggle_dt(&leds[0]);
            if (err < 0)
            {
                return;
            }
            LOG_INF("Button 1 pressed");
            button_s.button1 = true;
        }
        else
        {
            LOG_INF("Button 1 released");
            button_s.button1 = false;
        }
        break;
    case 2:
        if (evt == BUTTON_EVT_PRESSED)
        {
            LOG_INF("Button 2 pressed");
            button_s.button2 = true;
        }
        else
        {
            err = gpio_pin_toggle_dt(&leds[0]);
            if (err < 0)
            {
                return;
            }
            LOG_INF("Button 2 released");
            button_s.button2 = false;
        }
        break;
    case 3:
        if (evt == BUTTON_EVT_PRESSED)
        {
            LOG_INF("Button 3 pressed");
            button_s.button3 = true;

            int err_code = bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
            if (err_code)
            {
                LOG_INF("Cannot delete bond (err: %d)\n", err);
            }
            else
            {
                LOG_INF("Bond deleted succesfully");
            }
        }
        else
        {
            LOG_INF("Button 3 released");
            button_s.button3 = false;
        }
        break;
    default:
        LOG_ERR("Unknown button %zu event", idx + 1);
        break;
    }
}

int main(void)
{
    LOG_INF("initializing buttons");

    // Initialize buttons
    for (size_t i = 0; i < ARRAY_SIZE(buttons); i++)
    {
        int err = button_init(i, button_event_handler);
        if (err)
        {
            LOG_ERR("Button %zu Init failed: %d", i + 1, err);
            return err;
        }
    }

    LOG_INF("Init succeeded. Waiting for event...");

    k_thread_create(&batteryUpdateThread_data, batteryUpdateThread_stack_area,
                    K_THREAD_STACK_SIZEOF(batteryUpdateThread_stack_area),
                    batteryUpdateThread, NULL, NULL, NULL,
                    PRIORITY, 0, K_FOREVER);
    k_thread_name_set(&batteryUpdateThread_data, "batterythread");

    k_thread_start(&batteryUpdateThread_data);

    const struct device *const as = DEVICE_DT_GET(DT_INST(0, ams_as5600));

    if (as == NULL || !device_is_ready(as))
    {
        printk("as5600 device tree not configured\n");
        return;
    }

    int err;

    err = bt_enable(NULL);
    if (err)
    {
        LOG_INF("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    settings_load();

    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err)
    {
        LOG_INF("Advertising failed to start (err %d)\n", err);
    }

    LOG_INF("Advertising successfully started\n");

    if (IS_ENABLED(CONFIG_SAMPLE_BT_USE_AUTHENTICATION))
    {
        bt_conn_auth_cb_register(&auth_cb_display);
        LOG_INF("Bluetooth authentication callbacks registered.\n");
    }

    LOG_INF("system started");

    // Main loop
    while (1)
    {

        int degrees = as5600_refresh(as);
        int usefulDegrees = as5600_refresh(as);
        int lastDegree = as5600_refresh(as);
        int deltadegrees = 0;
        int lastDeltadegrees = 0;
        int deltaDeltadegrees = 0;
        int lastdeltaDeltadegrees = 0;
        deltadegrees = (degrees - lastDegree);
        deltaDeltadegrees = (deltadegrees - lastDeltadegrees);
        // printk ("%d\n",degrees);
        if (deltaDeltadegrees < -200)
        {
            usefulDegrees = lastDeltadegrees;
            deltaDeltadegrees = lastdeltaDeltadegrees;
        }
        else if (deltaDeltadegrees > 200)
        {
            usefulDegrees = lastDeltadegrees;
            deltaDeltadegrees = lastdeltaDeltadegrees;
        }
        else
        {
            usefulDegrees = (degrees - lastDegree);

            // printk("%d\n",usefulDegrees);
        }

        if (simulate_input)
        {
            /* HID Report:
             * Byte 0: buttons (lower 3 bits)
             * Byte 1: X axis (int8)
             * Byte 2: Y axis (int8)
             */
            uint8_t report[8] = {0,0,0,0,0,0,0,0};
            // report[0] = 0x02; //consumer control id

            if (degrees != lastDegree)
            {

                // report[1] = usefulDegrees * 10;
                // if (deltaDeltadegrees > 0){
                // 	// state += (deltaDeltadegrees);
                // 	report[1] = deltaDeltadegrees;
                // 	// LOG_INF("right");

                // }else{
                // 	// state += (deltaDeltadegrees);
                // 	report[1] = deltaDeltadegrees;
                // 	// LOG_INF("left");
                // }
            }

            lastDegree = degrees;
            lastDeltadegrees = deltadegrees;
            lastdeltaDeltadegrees = deltaDeltadegrees;

            if (button_s.button1)
            {
                
                // report[1] = -10;
                report[3] = 0x01;
                LOG_INF("up");
            }

            if (button_s.button0)
            {
                // report[1] = 10;
                report[3] = 0xFF;
                LOG_INF("down");
            }

            // if (button_s.button2)
            // {
            //     report[0] |= BIT(0);
            //     LOG_INF("left click");
            // }

            bt_gatt_notify(NULL, &hog_svc.attrs[13], report, sizeof(report));

            // 5 for keyboard
            // 9 for consumer
            // 13 for mouse
        }
    }

    return 0;
}
