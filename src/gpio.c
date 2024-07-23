#include "gpio.h"

LOG_MODULE_REGISTER(Button, LOG_LEVEL_DBG);

// LED specification
const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

// Button specifications
const struct gpio_dt_spec buttons[] = {
    GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0}),
    GPIO_DT_SPEC_GET_OR(SW1_NODE, gpios, {0}),
    GPIO_DT_SPEC_GET_OR(SW2_NODE, gpios, {0}),
    GPIO_DT_SPEC_GET_OR(SW3_NODE, gpios, {0}),
};

// Callback data and user callbacks
static struct gpio_callback button_cb_data[ARRAY_SIZE(buttons)];
static button_event_handler_t user_cb[ARRAY_SIZE(buttons)];

// Work item structure for debouncing
struct work_item {
    struct k_work_delayable work;
    size_t idx;
};

static struct work_item work_items[ARRAY_SIZE(buttons)];

// LED initialization
int ledInit()
{
    int ret;
    if (!gpio_is_ready_dt(&led)) {
        return 0;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return 0;
    }
    return ret;
}

// Cooldown expiration handler
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

// Button pressed handler
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

// Button initialization
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

    return 0;
}
