#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/usb/class/usb_hid.h>



#define CLOCKWISE 0x08 // clockwise key
#define COUNTERCLOCKWISE 0x09 // counter clockwise key

// 0xEA HID usage ID for volume decrement
// 0xE9 HID usage ID for volume increment
// HID_KEY_F12
// HID_KEY_F11


#define STACKSIZE 1024
#define PRIORITY 1
#define IDENT_OFFSET 1

K_THREAD_STACK_DEFINE(thread_stack, STACKSIZE);

static struct k_thread thread_data;
static const struct device *hid_device;

static const uint8_t composite_hid_report_desc[] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x06,       // Usage (Keyboard)
    0xA1, 0x01,       // Collection (Application)
    0x85, 0x01,       // Report ID (1)
    0x05, 0x07,       // Usage Page (Key Codes)
    0x19, 0xE0,       // Usage Minimum (224)
    0x29, 0xE7,       // Usage Maximum (231)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x01,       // Logical Maximum (1)
    0x75, 0x01,       // Report Size (1)
    0x95, 0x08,       // Report Count (8)
    0x81, 0x02,       // Input (Data, Variable, Absolute)
    0x95, 0x01,       // Report Count (1)
    0x75, 0x08,       // Report Size (8)
    0x81, 0x03,       // Input (Constant)
    0x95, 0x05,       // Report Count (5)
    0x75, 0x01,       // Report Size (1)
    0x05, 0x08,       // Usage Page (LEDs)
    0x19, 0x01,       // Usage Minimum (1)
    0x29, 0x05,       // Usage Maximum (5)
    0x91, 0x02,       // Output (Data, Variable, Absolute)
    0x95, 0x01,       // Report Count (1)
    0x75, 0x03,       // Report Size (3)
    0x91, 0x03,       // Output (Constant)
    0x95, 0x06,       // Report Count (6)
    0x75, 0x08,       // Report Size (8)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x65,       // Logical Maximum (101)
    0x05, 0x07,       // Usage Page (Key Codes)
    0x19, 0x00,       // Usage Minimum (0)
    0x29, 0x65,       // Usage Maximum (101)
    0x81, 0x00,       // Input (Data, Array)
    0xC0,             // End Collection

    // Consumer Control
    0x05, 0x0C,       // Usage Page (Consumer)
    0x09, 0x01,       // Usage (Consumer Control)
    0xA1, 0x01,       // Collection (Application)
    0x85, 0x02,       // Report ID (2)
    0x19, 0x00,       // Usage Minimum (0)
    0x2A, 0xFF, 0x03, // Usage Maximum (1023)
    0x15, 0x00,       // Logical Minimum (0)
    0x26, 0xFF, 0x03, // Logical Maximum (1023)
    0x75, 0x10,       // Report Size (16)
    0x95, 0x01,       // Report Count (1)
    0x81, 0x00,       // Input (Data, Array)
    0xC0,             // End Collection

    // Custom Buttons
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x00,       // Usage (Undefined)
    0xA1, 0x01,       // Collection (Application)
    0x85, 0x03,       // Report ID (3)
    0x05, 0x09,       // Usage Page (Button)
    0x19, 0x01,       // Usage Minimum (Button 1)
    0x29, 0x02,       // Usage Maximum (Button 2)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x01,       // Logical Maximum (1)
    0x75, 0x01,       // Report Size (1)
    0x95, 0x02,       // Report Count (2)
    0x81, 0x02,       // Input (Data, Variable, Absolute)
    0x95, 0x06,       // Report Count (6)
    0x81, 0x03,       // Input (Constant)
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
            uint8_t rep[] = {0x03, 0x00}; // Report ID 3, initial state

            if (deltadegrees > 0) {
                rep[2] = CLOCKWISE;
            } else {
                rep[2] = COUNTERCLOCKWISE;
            }

            k_sem_take(&usb_ready_sem, K_FOREVER);
            hid_int_ep_write(hid_device, rep, sizeof(rep), NULL);
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

    usb_hid_register_device(hid_device, composite_hid_report_desc, sizeof(composite_hid_report_desc), &ops);
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
            uint8_t rep[] = {0x03, 0x00}; // Report ID 3, release all keys

            k_sem_take(&usb_ready_sem, K_FOREVER);
            hid_int_ep_write(hid_device, rep, sizeof(rep), NULL);
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

    usb_hid_register_device(hid_device, composite_hid_report_desc, sizeof(composite_hid_report_desc), &ops);
    return usb_hid_init(hid_device);
}

SYS_INIT(composite_pre_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);



