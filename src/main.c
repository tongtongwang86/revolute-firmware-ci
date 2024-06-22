#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/usb/class/usb_hid.h>

#define KEY_1_CONFIGURE HID_KEY_Z // clockwise key
#define KEY_2_CONFIGURE HID_KEY_X // counterclockwise key
#define STACKSIZE 1024
#define PRIORITY 1
#define SLEEPTIME 500
#define IDENT_OFFSET 1
K_THREAD_STACK_DEFINE(threadA_stack_area, STACKSIZE);

static struct k_thread threadA_data;
static const struct device *hdev;

static const uint8_t hid_kbd_report_desc[] = HID_KEYBOARD_REPORT_DESC();

K_SEM_DEFINE(my_sem, 0, 10);
static K_SEM_DEFINE(usb_sem, 1, 1); // starts off "available"

static void int_in_ready_cb(const struct device *dev)
{
    ARG_UNUSED(dev);
    k_sem_give(&usb_sem);
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
        printk("Sample fetch error: %d\n", ret);
        return ret;
    }
    sensor_channel_get(dev, SENSOR_CHAN_ROTATION, &rot_raw);
    return rot_raw.val1;
}

void send_key_report(uint8_t key)
{
    uint8_t rep[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, key};
    k_sem_take(&usb_sem, K_FOREVER);
    int ret = hid_int_ep_write(hdev, rep, sizeof(rep), NULL);
    if (ret) {
        printk("Error sending key report: %d\n", ret);
    }
    k_sem_give(&my_sem);
}

void threadA(void *dummy1, void *dummy2, void *dummy3)
{
    const struct device *const as = DEVICE_DT_GET(DT_INST(0, ams_as5600));
    if (as == NULL || !device_is_ready(as)) {
        printk("AS5600 device not ready\n");
        return;
    }

    ARG_UNUSED(dummy1);
    ARG_UNUSED(dummy2);
    ARG_UNUSED(dummy3);

    int lastIdent = (as5600_refresh(as) - IDENT_OFFSET) / 12;
    int lastDegree = as5600_refresh(as);

    while (1) {
        int degrees = as5600_refresh(as);
        int deltadegrees = degrees - lastDegree;

        if (deltadegrees < -200) {
            deltadegrees += 360 + 50;
        } else if (deltadegrees > 200) {
            deltadegrees -= 360 + 50;
        }

        int currentIdent = (degrees + 6 + IDENT_OFFSET) / 12;
        if (lastIdent != currentIdent && currentIdent != 30) {
            uint8_t key = deltadegrees > 0 ? KEY_1_CONFIGURE : KEY_2_CONFIGURE;
            send_key_report(key);
            lastIdent = currentIdent;
            lastDegree = degrees;
        }
    }
}

BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
             "Console device is not ACM CDC UART device");

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
USBD_CONFIGURATION_DEFINE(config_1,
                          USB_SCD_SELF_POWERED,
                          200);

USBD_DESC_LANG_DEFINE(sample_lang);
USBD_DESC_MANUFACTURER_DEFINE(sample_mfr, "ZEPHYR");
USBD_DESC_PRODUCT_DEFINE(sample_product, "Zephyr USBD ACM console");
USBD_DESC_SERIAL_NUMBER_DEFINE(sample_sn, "0123456789ABCDEF");

USBD_DEVICE_DEFINE(sample_usbd,
                   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
                   0x2fe3, 0x0001);

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

    k_thread_create(&threadA_data, threadA_stack_area,
                    K_THREAD_STACK_SIZEOF(threadA_stack_area),
                    threadA, NULL, NULL, NULL,
                    PRIORITY, 0, K_FOREVER);
    k_thread_name_set(&threadA_data, "thread_a");
    k_thread_start(&threadA_data);

    while (1) {
        if (k_sem_take(&my_sem, K_MSEC(50)) == 0) {
            send_key_report(0x00); // Release all keys
        }
    }
}

static int composite_pre_init(void)
{
    hdev = device_get_binding("HID_0");
    if (hdev == NULL) {
        printk("Cannot get USB HID Device");
        return -ENODEV;
    }

    usb_hid_register_device(hdev, hid_kbd_report_desc, sizeof(hid_kbd_report_desc), &ops);

    return usb_hid_init(hdev);
}

SYS_INIT(composite_pre_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
