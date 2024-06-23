#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/usb/class/usb_hid.h>

#define VOLUME_UP 0xE9     // HID usage ID for volume increment
#define VOLUME_DOWN 0xEA   // HID usage ID for volume decrement
#define KEY_Z 0x1D         // HID usage ID for key 'z'
#define KEY_X 0x1B         // HID usage ID for key 'x'
#define STACKSIZE 1024
#define PRIORITY 1
#define SLEEPTIME 500
#define IDENT_OFFSET 1

K_THREAD_STACK_DEFINE(thread_stack, STACKSIZE);

static struct k_thread thread_data;
static const struct device *hid_device;
static bool use_keyboard = true;  // Variable to switch between keyboard and volume control

static const uint8_t hid_report_desc[] = {
    // Keyboard report descriptor
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x06,       // Usage (Keyboard)
    0xA1, 0x01,       // Collection (Application)
    0x85, 0x01,       // Report ID (1)
    0x05, 0x07,       // Usage Page (Keyboard)
    0x19, 0xE0,       // Usage Minimum (0xE0)
    0x29, 0xE7,       // Usage Maximum (0xE7)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x01,       // Logical Maximum (1)
    0x75, 0x01,       // Report Size (1)
    0x95, 0x08,       // Report Count (8)
    0x81, 0x02,       // Input (Data, Var, Abs)
    0x95, 0x01,       // Report Count (1)
    0x75, 0x08,       // Report Size (8)
    0x81, 0x03,       // Input (Const, Var, Abs)
    0x95, 0x06,       // Report Count (6)
    0x75, 0x08,       // Report Size (8)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x65,       // Logical Maximum (101)
    0x05, 0x07,       // Usage Page (Keyboard)
    0x19, 0x00,       // Usage Minimum (0)
    0x29, 0x65,       // Usage Maximum (101)
    0x81, 0x00,       // Input (Data, Ary, Abs)
    0xC0,             // End Collection

    // Consumer control report descriptor (volume control)
    0x05, 0x0C,       // Usage Page (Consumer)
    0x09, 0x01,       // Usage (Consumer Control)
    0xA1, 0x01,       // Collection (Application)
    0x85, 0x02,       // Report ID (2)
    0x09, 0xE9,       // Usage (Volume Increment)
    0x09, 0xEA,       // Usage (Volume Decrement)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x01,       // Logical Maximum (1)
    0x75, 0x01,       // Report Size (1)
    0x95, 0x02,       // Report Count (2)
    0x81, 0x02,       // Input (Data, Var, Abs)
    0x95, 0x06,       // Report Count (6)
    0x81, 0x03,       // Input (Cnst, Var, Abs)
    0xC0              // End Collection
};

K_SEM_DEFINE(data_ready_sem, 0, 10);
static K_SEM_DEFINE(usb_ready_sem, 1, 1); // starts off "available"

static void int_in_ready_cb(const struct device *dev)
{
    ARG_UNUSED(dev);
    k_sem_give(&usb_ready_sem);
}

static const struct hid_ops ops = {
    .int_in_ready = int_in_ready_cb,
};

int as5600_refresh(const struct device *dev)
{
    int ret;
    struct sensor_value rot_raw;
    ret = sensor_sample_fetch_chan(dev, SENSOR_CHAN_ROTATION);
    if (ret != 0) {
        printk("sample fetch error: %d\n", ret);
        return ret;
    }
    ret = sensor_channel_get(dev, SENSOR_CHAN_ROTATION, &rot_raw);
    if (ret != 0) {
        printk("channel get error: %d\n", ret);
        return ret;
    }
    return rot_raw.val1;
}

void thread_function(void *dummy1, void *dummy2, void *dummy3)
{
    ARG_UNUSED(dummy1);
    ARG_UNUSED(dummy2);
    ARG_UNUSED(dummy3);

    const struct device *const as = DEVICE_DT_GET(DT_INST(0, ams_as5600));

    if (as == NULL || !device_is_ready(as)) {
        printk("as5600 device tree not configured or not ready\n");
        return;
    }

    int last_identifier = (as5600_refresh(as) - (as5600_refresh(as) % 12)) / 12;
    int last_degree = as5600_refresh(as);

    while (1) {
        int degrees = as5600_refresh(as);
        int deltadegrees = 0;

        if (degrees < 0) {
            // Handle error in as5600_refresh
            continue;
        }

        if (degrees - last_degree < -200) {
            deltadegrees = (degrees - last_degree) + 360 + 50;
        } else if (degrees - last_degree > 200) {
            deltadegrees = (degrees - last_degree) - 360 - 50;
        } else {
            deltadegrees = (degrees - last_degree);
        }

        if (last_identifier != (((degrees + 6 + IDENT_OFFSET)) - ((degrees + 6 + IDENT_OFFSET) % 12)) / 12 &&
            (((degrees + 6 + IDENT_OFFSET)) - ((degrees + 6 + IDENT_OFFSET) % 12)) / 12 != 30) {
            uint8_t rep[] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Report ID 1, initial state for keyboard
            uint8_t volume_rep[] = {0x02, 0x00}; // Report ID 2, initial state for volume control

            if (use_keyboard) {
                if (deltadegrees > 0) {
                    rep[7] = KEY_Z; // Set key Z
                } else {
                    rep[7] = KEY_X; // Set key X
                }
            } else {
                if (deltadegrees > 0) {
                    volume_rep[1] = VOLUME_UP;
                } else {
                    volume_rep[1] = VOLUME_DOWN;
                }
            }

            k_sem_take(&usb_ready_sem, K_FOREVER);
            if (use_keyboard) {
                hid_int_ep_write(hid_device, rep, sizeof(rep), NULL);
            } else {
                hid_int_ep_write(hid_device, volume_rep, sizeof(volume_rep), NULL);
            }
            k_sem_give(&data_ready_sem);
            last_identifier = ((degrees + 6 + IDENT_OFFSET) - ((degrees + 6 + IDENT_OFFSET) % 12)) / 12;
            last_degree = degrees;
        }
    }
}

BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart), "Console device is not ACM CDC UART device");

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
USBD_CONFIGURATION_DEFINE(config_1, USB_SCD_SELF_POWERED, 200);
USBD_DESC_LANG_DEFINE(sample_lang);
USBD_DESC_MANUFACTURER_DEFINE(sample_mfr, "ZEPHYR");
USBD_DESC_PRODUCT_DEFINE(sample_product, "Zephyr USBD ACM console");
USBD_DESC_SERIAL_NUMBER_DEFINE(sample_sn, "0123456789ABCDEF");

USBD_DEVICE_DEFINE(sample_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), 0x2fe3, 0x0001);

static int enable_usb_device_next(void)
{
    usbd_add_descriptor(&sample_usbd, &sample_lang);
    usbd_add_descriptor(&sample_usbd, &sample_mfr);
    usbd_add_descriptor(&sample_usbd, &sample_product);
    usbd_add_descriptor(&sample_usbd, &sample_sn);
    usbd_add_configuration(&sample_usbd, &config_1);
    usbd_register_class(&sample_usbd, "cdc_acm_0", 1);
    usbd_init(&sample_usbd);
    usbd_enable(&sample_usbd);

    return 0;
}

#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK_NEXT) */

int main(void)
{
#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
    if (enable_usb_device_next()) {
        return 0;
    }
#else
    if (usb_enable(NULL)) {
        return 0;
    }
#endif

    if (!device_is_ready(DEVICE_DT_GET(DT_CHOSEN(zephyr_console)))) {
        printk("Console device is not ready\n");
        return 0;
    }

    hid_device = device_get_binding("HID_0");
    if (hid_device == NULL) {
        printk("Cannot get USB HID Device\n");
        return -ENODEV;
    }

    usb_hid_register_device(hid_device, hid_report_desc, sizeof(hid_report_desc), &ops);
    usb_hid_init(hid_device);

    k_sleep(K_SECONDS(1)); // Delay to ensure all initializations are complete

    k_thread_create(&thread_data, thread_stack,
                    K_THREAD_STACK_SIZEOF(thread_stack),
                    thread_function, NULL, NULL, NULL,
                    PRIORITY, 0, K_FOREVER);
    k_thread_name_set(&thread_data, "thread_a");

    k_thread_start(&thread_data);

    while (1) {
        if (k_sem_take(&data_ready_sem, K_MSEC(50)) != 0) {
            continue;
        } else {
            uint8_t rep[] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Report ID 1, release all keys
            uint8_t volume_rep[] = {0x02, 0x00}; // Report ID 2, release volume controls

            k_sem_take(&usb_ready_sem, K_FOREVER);
            if (use_keyboard) {
                hid_int_ep_write(hid_device, rep, sizeof(rep), NULL);
            } else {
                hid_int_ep_write(hid_device, volume_rep, sizeof(volume_rep), NULL);
            }
        }
    }
}

static int composite_pre_init(void)
{
    hid_device = device_get_binding("HID_0");
    if (hid_device == NULL) {
        printk("Cannot get USB HID Device");
        return -ENODEV;
    }

    usb_hid_register_device(hid_device, hid_report_desc, sizeof(hid_report_desc), &ops);
    return usb_hid_init(hid_device);
}

SYS_INIT(composite_pre_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
