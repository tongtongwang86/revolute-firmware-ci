#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>



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

struct button_data {
    struct gpio_callback cb;
    struct k_work_delayable work;
    button_event_handler_t handler;
    size_t idx;
};

static struct button_data button_data[ARRAY_SIZE(buttons)];

static void cooldown_expired(struct k_work *work)
{
    struct button_data *data = CONTAINER_OF(work, struct button_data, work);
    int val = gpio_pin_get_dt(&buttons[data->idx]);
    enum button_evt evt = val ? BUTTON_EVT_PRESSED : BUTTON_EVT_RELEASED;
    if (data->handler) {
        data->handler(data->idx, evt);
    }
}

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    struct button_data *data = CONTAINER_OF(cb, struct button_data, cb);
    k_work_reschedule(&data->work, K_MSEC(15));
}

int button_init(size_t idx, button_event_handler_t handler)
{
    if (idx >= ARRAY_SIZE(buttons)) {
        return -EINVAL;
    }

    if (!handler) {
        return -EINVAL;
    }

    struct button_data *data = &button_data[idx];
    data->handler = handler;
    data->idx = idx;

    if (!device_is_ready(buttons[idx].port)) {
        printk("Button %zu port not ready\n", idx);
        return -EIO;
    }

    int err = gpio_pin_configure_dt(&buttons[idx], GPIO_INPUT);
    if (err) {
        printk("Failed to configure button %zu: %d\n", idx, err);
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(&buttons[idx], GPIO_INT_EDGE_BOTH);
    if (err) {
        printk("Failed to configure interrupt for button %zu: %d\n", idx, err);
        return err;
    }

    gpio_init_callback(&data->cb, button_pressed, BIT(buttons[idx].pin));
    err = gpio_add_callback(buttons[idx].port, &data->cb);
    if (err) {
        printk("Failed to add callback for button %zu: %d\n", idx, err);
        return err;
    }

    k_work_init_delayable(&data->work, cooldown_expired);

    printk("Button %zu initialized\n", idx);

    // Initialize LED GPIO as output (assuming LEDs are used)
    if (!device_is_ready(leds[0].port)) {
        printk("LED port not ready\n");
        return -EIO;
    }

    err = gpio_pin_configure_dt(&leds[0], GPIO_OUTPUT_ACTIVE);
    if (err < 0) {
        printk("Failed to configure LED: %d\n", err);
        return err;
    }

    return 0;
}

static void button_event_handler(size_t idx, enum button_evt evt)
{
    switch (idx) {
        case 0:
            if (evt == BUTTON_EVT_PRESSED) {
                int err = gpio_pin_toggle_dt(&leds[0]);
                if (err < 0) {
                    return err;
                }
                printk("Button 1 pressed\n");
            } else {
                int err = gpio_pin_toggle_dt(&leds[0]);
                if (err < 0) {
                    return err;
                }
                printk("Button 1 released\n");
            }
            break;
        case 1:
            if (evt == BUTTON_EVT_PRESSED) {
                int err = gpio_pin_toggle_dt(&leds[0]);
                if (err < 0) {
                    return err;
                }
                printk("Button 2 pressed\n");
            } else {
                printk("Button 2 released\n");
            }
            break;
        case 2:
            if (evt == BUTTON_EVT_PRESSED) {
                printk("Button 3 pressed\n");
            } else {
                int err = gpio_pin_toggle_dt(&leds[0]);
                if (err < 0) {
                    return err;
                }
                printk("Button 3 released\n");
            }
            break;
        case 3:
            if (evt == BUTTON_EVT_PRESSED) {
                printk("Button 4 pressed\n");
            } else {
                printk("Button 4 released\n");
            }
            break;
        default:
            printk("Unknown button %zu event\n", idx + 1);
            break;
    }
}

int main(void)
{
    printk("Button Debouncing Sample!\n");

    // Initialize buttons
    for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
        int err = button_init(i, button_event_handler);
        if (err) {
            printk("Button %zu Init failed: %d\n", i + 1, err);
            return err;
        }
    }

    // Main loop
    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}
