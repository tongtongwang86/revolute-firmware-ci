#include "hog.h"

LOG_MODULE_REGISTER(Hog, LOG_LEVEL_DBG);

static bool notify_hid_enabled;
uint8_t ctrl_point;

struct bt_hids_info info = {
    .bcd_hid = 0x0111,      /* HID Class Spec 1.11 */
    .b_country_code = 0x00, /* Hardware target country */
    .flags = BT_HIDS_REMOTE_WAKE | BT_HIDS_NORMALLY_CONNECTABLE,
};

static const struct hids_report mouse_input = {
    .id = REPORT_ID_MOUSE,
    .type = BT_HIDS_REPORT_TYPE_INPUT,
};
static uint8_t input_report[] = {0x00, 0x00};

static const uint8_t hid_rep_desc[] = {

HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),     // Generic Desktop Controls
    HID_USAGE(HID_USAGE_GD_MOUSE),         // Mouse
    HID_COLLECTION(HID_COLLECTION_APPLICATION), // Application Collection
    HID_REPORT_ID(HID_REPORT_ID_MOUSE),
        HID_USAGE(HID_USAGE_GD_POINTER),   // Pointer
        HID_COLLECTION(HID_COLLECTION_PHYSICAL), // Physical Collection
            HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP), // Generic Desktop Controls
            HID_USAGE(HID_USAGE_GD_X),     // X Axis
            HID_USAGE(HID_USAGE_GD_Y),     // Y Axis
            HID_LOGICAL_MIN8(-127),         // Logical Minimum (-127)
            HID_LOGICAL_MAX8(127),          // Logical Maximum (127)
            HID_REPORT_SIZE(8),            // Report size (8 bits)
            HID_REPORT_COUNT(2),           // Report count (2 fields: X and Y)
            HID_INPUT(HID_MAIN_VAL_DATA | HID_MAIN_VAL_VAR | HID_MAIN_VAL_REL), // Relative data, variable
        HID_END_COLLECTION,                // End Physical Collection
    HID_END_COLLECTION                     // End Application Collection

};



static ssize_t read_hids_info(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data, sizeof(info));
}

static ssize_t read_hids_report_map(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, hid_rep_desc, sizeof(hid_rep_desc));
}

static ssize_t read_hids_mouse_input_report(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, input_report, sizeof(input_report));
}

static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    notify_hid_enabled = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static ssize_t read_hids_report_ref(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data, sizeof(struct hids_report));
}

static ssize_t write_ctrl_point(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    printk("Control point written\n");
    return len;
}


BT_GATT_SERVICE_DEFINE(hog_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, read_hids_info, NULL, &info),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ, BT_GATT_PERM_READ_ENCRYPT, read_hids_report_map, NULL, NULL),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_ENCRYPT, read_hids_mouse_input_report, NULL, NULL),
    BT_GATT_CCC(input_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ_ENCRYPT, read_hids_report_ref, NULL, &mouse_input),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT, BT_GATT_CHRC_WRITE_WITHOUT_RESP, BT_GATT_PERM_WRITE, NULL, write_ctrl_point, &ctrl_point),
);

int send_mouse_xy(int8_t x, int8_t y)
{
    if (!notify_hid_enabled) {
        return -EACCES;
    }

    uint8_t rep[2] = {x,y};

    // send mouse data over bluetooth
    return bt_gatt_notify(NULL, &hog_svc.attrs[5], rep, sizeof(rep));
}