#include "hog.h"


LOG_MODULE_REGISTER(Hog, LOG_LEVEL_DBG);

struct k_work button_action_work;
struct k_work keyboard_bt_action_work;
struct k_work mouse_bt_action_work;
struct k_work consumer_bt_action_work;
struct k_work revolute_bt_action_work;

static uint8_t input_report[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const struct hids_report input = {
    .id = REPORT_ID_KEYBOARD,
    .type = BT_HIDS_REPORT_TYPE_INPUT,
};

 uint8_t ctrl_point;
 uint8_t simulate_input;


static const struct hids_report consumer_input = {
    .id = REPORT_ID_CONSUMER,
    .type = BT_HIDS_REPORT_TYPE_INPUT,
};

static const struct hids_report mouse_input = {
    .id = REPORT_ID_MOUSE,
    .type = BT_HIDS_REPORT_TYPE_INPUT,
};

struct bt_hids_info info = {
    .bcd_hid = 0x0111,      /* HID Class Spec 1.11 */
    .b_country_code = 0x00, /* Hardware target country */
    .flags = BT_HIDS_REMOTE_WAKE | BT_HIDS_NORMALLY_CONNECTABLE,
};

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
    HID_LOGICAL_MAX16(0xFF, 0x00),
    HID_USAGE_MIN8(0x00),
    HID_USAGE_MAX8(0xFF),
    HID_REPORT_SIZE(0x08),
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
    HID_REPORT_COUNT(0x05),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),
    // Constant padding for the last 3 bits.
    HID_REPORT_SIZE(0x03),
    HID_REPORT_COUNT(0x01),
    HID_INPUT(ZMK_HID_MAIN_VAL_CONST | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),
    // Some OSes ignore pointer devices without X/Y data.
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
    HID_USAGE(HID_USAGE_GD_X),
    HID_USAGE(HID_USAGE_GD_Y),
    HID_LOGICAL_MIN8(-0x7F),
    HID_LOGICAL_MAX8(0x7F),
    HID_REPORT_SIZE(0x08),
    HID_REPORT_COUNT(0x02),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_REL),
    // Adding Resolution Multiplier for the Wheel
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
    HID_USAGE(HID_USAGE_GD_RESOLUTION_MULTIPLIER),
    HID_LOGICAL_MIN8(0x00),
    HID_LOGICAL_MAX8(0x0F), // 0x0Fdefault
    0x35,0x01, // physical min 8
    0x45,0xFF, // physical max 8 //0x10 default
    HID_REPORT_SIZE(0x04),
    HID_REPORT_COUNT(0x01),
    HID_FEATURE(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),
    // Wheel Control
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
    HID_USAGE(HID_USAGE_GD_WHEEL),
    HID_LOGICAL_MIN8(-0x7F),
    HID_LOGICAL_MAX8(0x7F),
    HID_REPORT_COUNT(0x01),
    HID_REPORT_SIZE(0x08),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_REL),
    HID_END_COLLECTION,
    HID_END_COLLECTION,
};


static ssize_t read_hids_info(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data, sizeof(info));
}

static ssize_t read_hids_report_map(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, zmk_hid_report_desc, sizeof(zmk_hid_report_desc));
}

static ssize_t read_hids_input_report(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, input_report, sizeof(input_report));
}

static ssize_t read_hids_consumer_input_report(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, input_report, sizeof(input_report));
}

static ssize_t read_hids_mouse_input_report(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, input_report, sizeof(input_report));
}

static ssize_t write_ctrl_point(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    printk("Control point written\n");
    return len;
}

static ssize_t read_hids_report_ref(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data, sizeof(struct hids_report));
}

static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    simulate_input = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

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
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT, BT_GATT_CHRC_WRITE_WITHOUT_RESP, BT_GATT_PERM_WRITE, NULL, write_ctrl_point, &ctrl_point),
);



// send keyboard 


K_MSGQ_DEFINE(keyboard_action_msgq, sizeof(struct hid_keyboard_report_body), KEYBOARD_MSGQ_ARRAY_SIZE, 4);

void keyboard_bt_action_work_handler(struct k_work *work) {
    struct hid_keyboard_report_body report;
    while (k_msgq_num_used_get(&keyboard_action_msgq)) {
        k_msgq_get(&keyboard_action_msgq, &report, K_NO_WAIT);
        if (simulate_input) {

            uint8_t rep[8] = {report.modifiers,0,0,0,0,0,0,report.keys};

            bt_gatt_notify(NULL, &hog_svc.attrs[5], rep, sizeof(rep));
        }
        k_yield();
    }
}

void keyboard_bt_submit(struct hid_keyboard_report_body *report) {
    k_msgq_put(&keyboard_action_msgq, report, K_NO_WAIT);
    k_work_submit(&keyboard_bt_action_work);
}


// send consumer


K_MSGQ_DEFINE(consumer_action_msgq, sizeof(struct hid_consumer_report_body), CONSUMER_MSGQ_ARRAY_SIZE, 4);

void consumer_bt_action_work_handler(struct k_work *work) {
    struct hid_consumer_report_body report;
    while (k_msgq_num_used_get(&consumer_action_msgq)) {
        k_msgq_get(&consumer_action_msgq, &report, K_NO_WAIT);
        if (simulate_input) {

            uint8_t rep[6] = {report.keys,0,0,0,0,0};

            bt_gatt_notify(NULL, &hog_svc.attrs[9], rep, sizeof(rep));
        }
        k_yield();
    }
}

void consumer_bt_submit(struct hid_consumer_report_body *report) {
    k_msgq_put(&consumer_action_msgq, report, K_NO_WAIT);
    k_work_submit(&consumer_bt_action_work);
}

// send mouse

K_MSGQ_DEFINE(mouse_action_msgq, sizeof(struct hid_mouse_report_body), MOUSE_MSGQ_ARRAY_SIZE, 4);

void mouse_bt_action_work_handler(struct k_work *work) {
    struct hid_mouse_report_body report;
    while (k_msgq_num_used_get(&mouse_action_msgq)) {
        k_msgq_get(&mouse_action_msgq, &report, K_NO_WAIT);
        if (simulate_input) {

            uint8_t rep[8] = {report.buttons,report.d_x,report.d_y,report.d_wheel,0,0,0,0};

            bt_gatt_notify(NULL, &hog_svc.attrs[13], rep, sizeof(rep));
        }
        k_yield();
    }
}

void mouse_bt_submit(struct hid_mouse_report_body *report) {
    k_msgq_put(&mouse_action_msgq, report, K_NO_WAIT);
    k_work_submit(&mouse_bt_action_work);
}




K_MSGQ_DEFINE(revolute_action_msgq, sizeof(struct hid_revolute_report_body), REVOLUTE_MSGQ_ARRAY_SIZE, 4);

void revolute_bt_action_work_handler(struct k_work *work) {
    struct hid_revolute_report_body report;
    while (k_msgq_num_used_get(&revolute_action_msgq)) {
        k_msgq_get(&revolute_action_msgq, &report, K_NO_WAIT);
        if (simulate_input) {

            uint8_t rep[8] = {report.report[0],report.report[1],report.report[2],report.report[3],report.report[4],report.report[5],report.report[6],report.report[7]};
            // printk("%d",report.id);
            bt_gatt_notify(NULL, &hog_svc.attrs[report.id], rep, sizeof(rep));
        }
        k_yield();
    }
}

void revolute_bt_submit(struct hid_revolute_report_body *report) {
    k_msgq_put(&revolute_action_msgq, report, K_NO_WAIT);
    k_work_submit(&revolute_bt_action_work);
}