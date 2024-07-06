#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define SW0_NODE    DT_ALIAS(sw0)
#define LED0_NODE   DT_ALIAS(led0)

enum button_evt {
    BUTTON_EVT_PRESSED,
    BUTTON_EVT_RELEASED
};

typedef void (*button_event_handler_t)(enum button_evt evt);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static struct gpio_callback button_cb_data;

static button_event_handler_t user_cb;

static void cooldown_expired(struct k_work *work)
{
    ARG_UNUSED(work);

    int val = gpio_pin_get_dt(&button);
    enum button_evt evt = val ? BUTTON_EVT_PRESSED : BUTTON_EVT_RELEASED;
    if (user_cb) {
        user_cb(evt);
    }
}

static K_WORK_DELAYABLE_DEFINE(cooldown_work, cooldown_expired);

void button_pressed(const struct device *dev, struct gpio_callback *cb,
                    uint32_t pins)
{
    k_work_reschedule(&cooldown_work, K_MSEC(15));
}

int button_init(button_event_handler_t handler)
{
    if (!handler) {
        return -EINVAL;
    }

    user_cb = handler;

    if (!device_is_ready(button.port)) {
        return -EIO;
    }

    int err = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (err) {
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
    if (err) {
        return err;
    }

    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    err = gpio_add_callback(button.port, &button_cb_data);
    if (err) {
        return err;
    }

    return 0;
}

static char *helper_button_evt_str(enum button_evt evt)
{
    int err;
    switch (evt) {
    case BUTTON_EVT_PRESSED:
        err = gpio_pin_toggle_dt(&led);
        if (err < 0) {
            return "Error";
        }
        return "Pressed";
    case BUTTON_EVT_RELEASED:
        err = gpio_pin_toggle_dt(&led);
        if (err < 0) {
            return "Error";
        }
        return "Released";
    default:
        return "Unknown";
    }
}

static void button_event_handler(enum button_evt evt)
{
    printk("Button event: %s\n", helper_button_evt_str(evt));
}

int main(void)
{
    printk("Button Debouncing Sample!\n");

    int err = button_init(button_event_handler);
    if (err) {
        printk("Button Init failed: %d\n", err);
        return err;
    }

    if (!device_is_ready(led.port)) {
        return -EIO;
    }

    err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (err < 0) {
        return err;
    }

    printk("Init succeeded. Waiting for event...\n");
    return 0;
}
