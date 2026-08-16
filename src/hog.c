#include "hog.h"
#include "revsvc.h"
// #include "hid_usage.h"
// #include "hid_usage_pages.h"

LOG_MODULE_REGISTER(hog, LOG_LEVEL_DBG);

#define REPORT_ID_KEYBOARD 1
#define REPORT_ID_CONSUMER 2
#define REPORT_ID_MOUSE 3
#define REPORT_ID_CONTROLLER 4
#define REVOLUTE_MSGQ_ARRAY_SIZE 32

#define ZMK_HID_REPORT_ID_KEYBOARD 0x01
#define ZMK_HID_REPORT_ID_LEDS 0x01
#define ZMK_HID_REPORT_ID_CONSUMER 0x02
#define ZMK_HID_REPORT_ID_MOUSE 0x03
#define ZMK_HID_REPORT_ID_CONTROLLER 0x04


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

enum {
    HIDS_INPUT = 0x01,
    HIDS_OUTPUT = 0x02,
    HIDS_FEATURE = 0x03,
};

static uint8_t input_report[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

enum {
    BT_HIDS_REMOTE_WAKE = BIT(0),
    BT_HIDS_NORMALLY_CONNECTABLE = BIT(1),
};

/* One rotation tick as it travels from the sensor thread to the BLE sender.
 * A tick carries its own report bytes so the values are captured at the moment
 * the tick is detected, not at the moment it happens to be transmitted, and it
 * carries the release flag so a press can never be queued without its release.
 */
struct hid_revolute_tick {
    uint8_t transport; /* attribute index of the HID report characteristic */
    uint8_t report[8];
    bool release; /* send an all-zero report after the payload */
};

K_MSGQ_DEFINE(revolute_msgq, sizeof(struct hid_revolute_tick), REVOLUTE_MSGQ_ARRAY_SIZE, 4);

struct hids_report
{
    uint8_t id;   /* report id */
    uint8_t type; /* report type */
} __packed;

static const struct hids_report input = {
    .id = REPORT_ID_KEYBOARD,
    .type = HIDS_INPUT,
};


static const struct hids_report consumer_input = {
    .id = REPORT_ID_CONSUMER,
    .type = HIDS_INPUT,
};

static const struct hids_report mouse_input = {
    .id = REPORT_ID_MOUSE,
    .type = HIDS_INPUT,
};

static const struct hids_report controller_input = {
    .id = REPORT_ID_CONTROLLER,
    .type = HIDS_INPUT,
};

struct hids_info {
    uint16_t version; /* version number of base USB HID Specification */
    uint8_t code;     /* country HID Device hardware is localized for. */
    uint8_t flags;
} __packed;

static struct hids_info info = {
    .version = 0x0111,
    .code = 0x00,
    .flags = BT_HIDS_REMOTE_WAKE | BT_HIDS_NORMALLY_CONNECTABLE,
};



/* Notification state per HID report. Sharing one flag across all four CCC
 * descriptors meant a host unsubscribing from any single report silenced every
 * report, so each one tracks its own subscription. */
enum revolute_report_idx {
    REVOLUTE_RPT_KEYBOARD,
    REVOLUTE_RPT_CONSUMER,
    REVOLUTE_RPT_MOUSE,
    REVOLUTE_RPT_CONTROLLER,
    REVOLUTE_RPT_COUNT,
};

static uint8_t notify_enabled[REVOLUTE_RPT_COUNT];
static uint8_t ctrl_point;
static uint8_t report_map[] = {
    0x05, 0x01,  // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,  // USAGE (Keyboard)
    0xA1, 0x01,  // COLLECTION (Application)
    0x85, 0x01,  // REPORT_ID (1)
    0x05, 0x07,  // USAGE_PAGE (Key Codes)
    0x19, 0xE0,  // USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xE7,  // USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00,  // LOGICAL_MINIMUM (0)
    0x25, 0x01,  // LOGICAL_MAXIMUM (1)
    0x75, 0x01,  // REPORT_SIZE (1)
    0x95, 0x08,  // REPORT_COUNT (8)
    0x81, 0x02,  // INPUT (Data,Var,Abs)
    0x75, 0x08,  // REPORT_SIZE (8)
    0x95, 0x01,  // REPORT_COUNT (1)
    0x81, 0x03,  // INPUT (Cnst,Var,Abs)
    0x19, 0x00,  // USAGE_MINIMUM (0)
    0x29, 0xFF,  // USAGE_MAXIMUM (255)
    0x15, 0x00,  // LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0x00,  // LOGICAL_MAXIMUM (255)
    0x75, 0x08,  // REPORT_SIZE (8)
    0x95, 0x06,  // REPORT_COUNT (6)
    0x81, 0x00,  // INPUT (Data,Array,Abs)
    0xC0,        // END_COLLECTION

    // Consumer Control
    0x05, 0x0C,  // USAGE_PAGE (Consumer)
    0x09, 0x01,  // USAGE (Consumer Control)
    0xA1, 0x01,  // COLLECTION (Application)
    0x85, 0x02,  // REPORT_ID (2)
    0x15, 0x00,  // LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0x00,  // LOGICAL_MAXIMUM (255)
    0x19, 0x00,  // USAGE_MINIMUM (0)
    0x29, 0xFF,  // USAGE_MAXIMUM (255)
    0x75, 0x08,  // REPORT_SIZE (8)
    0x95, 0x06,  // REPORT_COUNT (6)
    0x81, 0x00,  // INPUT (Data,Array,Abs)
    0xC0,        // END_COLLECTION

    // Mouse
    0x05, 0x01,        // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,        // USAGE (Mouse)
    0xa1, 0x01,        // COLLECTION (Application)
    0x85, 0x03,        // REPORT_ID (3)
    0x09, 0x02,        //   USAGE (Mouse)
    0xa1, 0x02,        //   COLLECTION (Logical)
    0x09, 0x01,        //     USAGE (Pointer)
    0xa1, 0x00,        //     COLLECTION (Physical)
    // ------------------------------  Buttons
    0x05, 0x09,        //       USAGE_PAGE (Button)
    0x19, 0x01,        //       USAGE_MINIMUM (Button 1)
    0x29, 0x05,        //       USAGE_MAXIMUM (Button 5)
    0x15, 0x00,        //       LOGICAL_MINIMUM (0)
    0x25, 0x01,        //       LOGICAL_MAXIMUM (1)
    0x75, 0x01,        //       REPORT_SIZE (1)
    0x95, 0x05,        //       REPORT_COUNT (5 Buttons)
    0x81, 0x02,        //       INPUT (Data,Var,Abs)
    // ------------------------------  Padding
    0x75, 0x03,        //       REPORT_SIZE (8-5buttons 3)
    0x95, 0x01,        //       REPORT_COUNT (1)
    0x81, 0x03,        //       INPUT (Cnst,Var,Abs)
    // ------------------------------  X,Y position
    0x05, 0x01,        //       USAGE_PAGE (Generic Desktop)
    0x09, 0x30,        //       USAGE (X)
    0x09, 0x31,        //       USAGE (Y)
    0x15, 0x81,        //       LOGICAL_MINIMUM (-127)
    0x25, 0x7f,        //       LOGICAL_MAXIMUM (127)
    0x75, 0x08,        //       REPORT_SIZE (8)
    0x95, 0x02,        //       REPORT_COUNT (2)
    0x81, 0x06,        //       INPUT (Data,Var,Rel)
    0xa1, 0x02,        //       COLLECTION (Logical)
    // ------------------------------  Vertical wheel res multiplier
    0x09, 0x48,        //         USAGE (Resolution Multiplier)
    0x15, 0x00,        //         LOGICAL_MINIMUM (0)
    0x25, 0x01,        //         LOGICAL_MAXIMUM (1)
    0x35, 0x01,        //         PHYSICAL_MINIMUM (1)
    0x45, 0x08,        //         PHYSICAL_MAXIMUM (8)
    0x75, 0x02,        //         REPORT_SIZE (2)
    0x95, 0x01,        //         REPORT_COUNT (1)
    0xa4,              //         PUSH
    0xb1, 0x02,        //         FEATURE (Data,Var,Abs)
    // ------------------------------  Vertical wheel
    0x09, 0x38,        //         USAGE (Wheel)
    0x15, 0x81,        //         LOGICAL_MINIMUM (-127)
    0x25, 0x7f,        //         LOGICAL_MAXIMUM (127)
    0x35, 0x00,        //         PHYSICAL_MINIMUM (0)        - reset physical
    0x45, 0x00,        //         PHYSICAL_MAXIMUM (0)
    0x75, 0x08,        //         REPORT_SIZE (8)
    0x81, 0x06,        //         INPUT (Data,Var,Rel)
    0xc0,              //       END_COLLECTION
    0xa1, 0x02,        //       COLLECTION (Logical)
    // ------------------------------  Horizontal wheel res multiplier
    0x09, 0x48,        //         USAGE (Resolution Multiplier)
    0xb4,              //         POP
    0xb1, 0x02,        //         FEATURE (Data,Var,Abs)
    // ------------------------------  Padding for Feature report
    0x35, 0x00,        //         PHYSICAL_MINIMUM (0)        - reset physical
    0x45, 0x00,        //         PHYSICAL_MAXIMUM (0)
    0x75, 0x04,        //         REPORT_SIZE (4)
    0xb1, 0x03,        //         FEATURE (Cnst,Var,Abs)
    // ------------------------------  Horizontal wheel
    0x05, 0x0c,        //         USAGE_PAGE (Consumer Devices)
    0x0a, 0x38, 0x02,  //         USAGE (AC Pan)
    0x15, 0x81,        //         LOGICAL_MINIMUM (-127)
    0x25, 0x7f,        //         LOGICAL_MAXIMUM (127)
    0x75, 0x08,        //         REPORT_SIZE (8)
    0x81, 0x06,        //         INPUT (Data,Var,Rel)
    0xc0,              //       END_COLLECTION
    0xc0,              //     END_COLLECTION
    0xc0,              //   END_COLLECTION
    0xc0,               // END_COLLECTION


    // // Game Controller
    // 0x05, 0x01,  // USAGE_PAGE (Generic Desktop)
    // 0x09, 0x05,  // USAGE (Game Pad)
    // 0xA1, 0x01,  // COLLECTION (Application)
    // 0x85, 0x04,  // REPORT_ID (4)
    // 0x09, 0x30,  // USAGE (X)
    // 0x09, 0x31,  // USAGE (Y)
    // 0x09, 0x32,  // USAGE (Z)
    // 0x09, 0x33,  // USAGE (Rx)
    // 0x15, 0x81,  // LOGICAL_MINIMUM (-127)
    // 0x25, 0x7F,  // LOGICAL_MAXIMUM (127)
    // 0x75, 0x08,  // REPORT_SIZE (8)
    // 0x95, 0x04,  // REPORT_COUNT (4)
    // 0x81, 0x02,  // INPUT (Data,Var,Abs)
    // 0x05, 0x09,  // USAGE_PAGE (Button)
    // 0x19, 0x01,  // USAGE_MINIMUM (1)
    // 0x29, 0x10,  // USAGE_MAXIMUM (16)
    // 0x15, 0x00,  // LOGICAL_MINIMUM (0)
    // 0x25, 0x01,  // LOGICAL_MAXIMUM (1)
    // 0x75, 0x01,  // REPORT_SIZE (1)
    // 0x95, 0x10,  // REPORT_COUNT (16)
    // 0x81, 0x02,  // INPUT (Data,Var,Abs)
    // 0x75, 0x08,  // REPORT_SIZE (8)
    // 0x95, 0x01,  // REPORT_COUNT (1)
    // 0x81, 0x03,  // INPUT (Cnst,Var,Abs)
    // 0xC0         // END_COLLECTION
};

static ssize_t read_hids_info(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data, sizeof(info));
}

static ssize_t read_hids_report_map(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, report_map, sizeof(report_map));
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

static ssize_t read_hids_controller_input_report(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, input_report, sizeof(input_report));
}

static ssize_t write_ctrl_point(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    printk("Control point written\n");
    return len;
}

static ssize_t read_hids_report_ref(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data, sizeof(struct hids_report));
}

static void keyboard_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    notify_enabled[REVOLUTE_RPT_KEYBOARD] = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static void consumer_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    notify_enabled[REVOLUTE_RPT_CONSUMER] = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static void mouse_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    notify_enabled[REVOLUTE_RPT_MOUSE] = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static void controller_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    notify_enabled[REVOLUTE_RPT_CONTROLLER] = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}


/* HID Service Declaration */
BT_GATT_SERVICE_DEFINE(hog_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, read_hids_info, NULL, &info),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ, BT_GATT_PERM_READ_ENCRYPT, read_hids_report_map, NULL, NULL),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_ENCRYPT, read_hids_input_report, NULL, NULL),
    BT_GATT_CCC(keyboard_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ_ENCRYPT, read_hids_report_ref, NULL, &input),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_ENCRYPT, read_hids_consumer_input_report, NULL, NULL),
    BT_GATT_CCC(consumer_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ_ENCRYPT, read_hids_report_ref, NULL, &consumer_input),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_ENCRYPT, read_hids_mouse_input_report, NULL, NULL),
    BT_GATT_CCC(mouse_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ_ENCRYPT, read_hids_report_ref, NULL, &mouse_input),


    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_ENCRYPT, read_hids_controller_input_report, NULL, NULL),
    BT_GATT_CCC(controller_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ_ENCRYPT, read_hids_report_ref, NULL, &controller_input),


    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT, BT_GATT_CHRC_WRITE_WITHOUT_RESP, BT_GATT_PERM_WRITE, NULL, write_ctrl_point, &ctrl_point),
);


/* ---------------------------------------------------------------------------
 * Rotation ticks -> BLE notifications
 *
 * Ticks used to be pushed straight into four static k_work items. k_work_submit()
 * is a no-op when the item is already pending, so any tick that arrived while the
 * previous notification was still in flight was silently discarded -- a fast spin
 * collapsed into a handful of events, and because press and release were separate
 * work items they could be dropped independently, leaving a stuck key.
 *
 * Ticks now go through a message queue drained by one dedicated sender thread.
 * Press and release travel together in a single queue entry, the report bytes are
 * captured when the tick is detected, and bt_gatt_notify()'s return code is
 * checked and retried instead of being thrown away.
 * ------------------------------------------------------------------------ */

#define REVOLUTE_SENDER_STACK_SIZE 1024
#define REVOLUTE_SENDER_PRIORITY   3
#define REVOLUTE_NOTIFY_RETRIES    10
#define REVOLUTE_NOTIFY_BACKOFF    K_MSEC(2)

static uint32_t revolute_dropped_ticks;

/* Map a HID report characteristic attribute index onto its notify_enabled slot.
 * Transports are the attribute indices of the four report characteristics:
 * 5 = keyboard, 9 = consumer, 13 = mouse, 17 = controller. */
static int transport_to_report(uint8_t transport)
{
    if (transport < 5 || transport > 17 || ((transport - 5) % 4) != 0) {
        return -EINVAL;
    }
    return (transport - 5) / 4;
}

static bool transport_notifiable(uint8_t transport)
{
    int idx = transport_to_report(transport);

    return (idx >= 0) && notify_enabled[idx];
}

/* Send one report, retrying while the controller's TX buffers are full.
 * -ENOMEM/-EAGAIN mean "no buffer right now", which is exactly the condition a
 * fast spin creates, so those are worth waiting out rather than dropping. */
static int revolute_notify(uint8_t transport, const uint8_t report[8])
{
    for (int attempt = 0; attempt < REVOLUTE_NOTIFY_RETRIES; attempt++) {
        int err = bt_gatt_notify(NULL, &hog_svc.attrs[transport], report, 8);

        if (err == 0) {
            return 0;
        }

        if (err != -ENOMEM && err != -EAGAIN && err != -ENOBUFS) {
            /* Not connected, not subscribed, or a permanent error. */
            return err;
        }

        k_sleep(REVOLUTE_NOTIFY_BACKOFF);
    }

    return -ENOMEM;
}

static void revolute_sender_thread(void *unused1, void *unused2, void *unused3)
{
    static const uint8_t release_report[8] = {0};
    struct hid_revolute_tick tick;

    ARG_UNUSED(unused1);
    ARG_UNUSED(unused2);
    ARG_UNUSED(unused3);

    while (1) {
        k_msgq_get(&revolute_msgq, &tick, K_FOREVER);

        if (!transport_notifiable(tick.transport)) {
            continue;
        }

        int err = revolute_notify(tick.transport, tick.report);
        if (err) {
            LOG_WRN("report notify failed (attr %u, err %d)", tick.transport, err);
        }

        /* Always attempt the release, even if the press failed, so the host can
         * never be left holding a key down. */
        if (tick.release) {
            err = revolute_notify(tick.transport, release_report);
            if (err) {
                LOG_WRN("release notify failed (attr %u, err %d)", tick.transport, err);
            }
        }
    }
}

K_THREAD_DEFINE(revolute_sender_tid, REVOLUTE_SENDER_STACK_SIZE, revolute_sender_thread,
                NULL, NULL, NULL, REVOLUTE_SENDER_PRIORITY, 0, 0);

static void revolute_submit(uint8_t transport, const uint8_t report[8], bool release)
{
    struct hid_revolute_tick tick = {
        .transport = transport,
        .release = release,
    };

    if (!transport_notifiable(transport)) {
        return;
    }

    memcpy(tick.report, report, sizeof(tick.report));

    if (k_msgq_put(&revolute_msgq, &tick, K_NO_WAIT) == 0) {
        return;
    }

    /* Queue full: the link is further behind than the buffer is deep. Drop the
     * oldest tick rather than the newest so the queue keeps tracking the most
     * recent motion, and keep a count so the overflow is visible. */
    struct hid_revolute_tick discard;

    if (k_msgq_get(&revolute_msgq, &discard, K_NO_WAIT) == 0) {
        revolute_dropped_ticks++;
    }

    (void)k_msgq_put(&revolute_msgq, &tick, K_NO_WAIT);
}

uint32_t revolute_dropped_tick_count(void)
{
    return revolute_dropped_ticks;
}

void revolute_up_submit(void)
{
    revolute_submit(config.up_transport, config.up_report, true);
}

void revolute_dn_submit(void)
{
    revolute_submit(config.dn_transport, config.dn_report, true);
}

/* Continuous (relative) reports: the configured report acts as a template and the
 * first nonzero byte carries the signed magnitude. The magnitude is substituted
 * here, at detection time, rather than being read from a shared global when the
 * report is eventually transmitted. */
static void revolute_cont_submit(uint8_t transport, const uint8_t template[8], int8_t delta)
{
    uint8_t report[8];

    memcpy(report, template, sizeof(report));

    for (int i = 0; i < 8; i++) {
        if (report[i] != 0) {
            report[i] = (uint8_t)delta;
            break;
        }
    }

    revolute_submit(transport, report, false);
}

void revolute_up_cont_submit(int8_t delta)
{
    revolute_cont_submit(config.up_transport, config.up_report, delta);
}

void revolute_dn_cont_submit(int8_t delta)
{
    revolute_cont_submit(config.dn_transport, config.dn_report, delta);
}
