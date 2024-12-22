#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(button_handler, LOG_LEVEL_DBG);

#define SW0_NODE    DT_ALIAS(sw0)
#define SW1_NODE    DT_ALIAS(sw1)
#define SW2_NODE    DT_ALIAS(sw2)
#define SW3_NODE    DT_ALIAS(sw3)
#define LED0_NODE   DT_ALIAS(led0)
#define MOS_NODE   DT_ALIAS(mos)

enum button_evt {
    BUTTON_EVT_PRESSED,
    BUTTON_EVT_RELEASED
};

typedef void (*button_event_handler_t)(size_t idx, enum button_evt evt);

static const struct gpio_dt_spec leds[] = {
    GPIO_DT_SPEC_GET_OR(MOS_NODE, gpios, {0}),

};

static const struct gpio_dt_spec buttons[] = {
    GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0}),
    GPIO_DT_SPEC_GET_OR(SW1_NODE, gpios, {0}),
    GPIO_DT_SPEC_GET_OR(SW2_NODE, gpios, {0}),
    GPIO_DT_SPEC_GET_OR(SW3_NODE, gpios, {0}),
};

static struct gpio_callback button_cb_data[ARRAY_SIZE(buttons)];
static button_event_handler_t user_cb[ARRAY_SIZE(buttons)];

struct work_item {
    struct k_work_delayable work;
    size_t idx;
};

static struct work_item work_items[ARRAY_SIZE(buttons)];

static void cooldown_expired(struct k_work *work)
{
    struct work_item *item = CONTAINER_OF(work, struct work_item, work.work);
    size_t idx = item->idx;
    int val = gpio_pin_get_dt(&buttons[idx]);
    enum button_evt evt = val ? BUTTON_EVT_PRESSED : BUTTON_EVT_RELEASED;

    if (user_cb[idx]) {
        user_cb[idx](idx, evt);
    }
}

void button_pressed_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    size_t idx = 0;
    for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
        if (cb == &button_cb_data[i]) {
            idx = i;
            break;
        }
    }

    k_work_reschedule(&work_items[idx].work, K_MSEC(15));
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

    // Initialize work items
    work_items[idx].idx = idx;
    k_work_init_delayable(&work_items[idx].work, cooldown_expired);

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
                    return;
                }
                LOG_INF("Button 1 pressed");
            } else {
                err = gpio_pin_toggle_dt(&leds[0]);
                if (err < 0) {
                    return;
                }
                LOG_INF("Button 0 released");
            }
            break;
        case 1:
            if (evt == BUTTON_EVT_PRESSED) {
                err = gpio_pin_toggle_dt(&leds[0]);
                if (err < 0) {
                    return;
                }
                LOG_INF("Button 1 pressed");
            } else {
                LOG_INF("Button 1 released");
            }
            break;
        case 2:
            if (evt == BUTTON_EVT_PRESSED) {
                LOG_INF("Button 2 pressed");
            } else {
                err = gpio_pin_toggle_dt(&leds[0]);
                if (err < 0) {
                    return;
                }
                LOG_INF("Button 2 released");
            }
            break;
        case 3:
            if (evt == BUTTON_EVT_PRESSED) {
                err = gpio_pin_toggle_dt(&leds[0]);
                LOG_INF("Button 3 pressed");
            } else {
                LOG_INF("Button 3 released");
            }
            break;
        default:
            LOG_ERR("Unknown button %zu event", idx + 1);
            break;
    }
}

int main(void)
{
    LOG_INF("Button Debouncing Sample!");

    // Initialize buttons
    for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
        int err = button_init(i, button_event_handler);
        if (err) {
            LOG_ERR("Button %zu Init failed: %d", i + 1, err);
            return err;
        }
    }

    LOG_INF("Init succeeded. Waiting for event...");

    // Main loop
    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}
