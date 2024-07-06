#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

// Define aliases for device tree nodes
#define SW0_NODE    DT_ALIAS(sw0)
#define SW1_NODE    DT_ALIAS(sw1)
#define LED0_NODE   DT_ALIAS(led0)

// Enum for button events
enum button_evt {
    BUTTON_EVT_PRESSED,
    BUTTON_EVT_RELEASED
};

// Typedef for button event handler
typedef void (*button_event_handler_t)(enum button_evt evt);

// GPIO specifications for LED and buttons
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec button0 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET(SW1_NODE, gpios);
static struct gpio_callback button0_cb_data;
static struct gpio_callback button1_cb_data;

// User-defined button event handler
static button_event_handler_t user_cb;

// Work item for cooldown period
static void cooldown_expired(struct k_work *work)
{
    ARG_UNUSED(work);

    int val0 = gpio_pin_get_dt(&button0);
    int val1 = gpio_pin_get_dt(&button1);

    enum button_evt evt0 = val0 ? BUTTON_EVT_PRESSED : BUTTON_EVT_RELEASED;
    enum button_evt evt1 = val1 ? BUTTON_EVT_PRESSED : BUTTON_EVT_RELEASED;

    if (user_cb) {
        if (evt0 == BUTTON_EVT_PRESSED || evt1 == BUTTON_EVT_PRESSED) {
            user_cb(BUTTON_EVT_PRESSED);
        }
    }
}

static K_WORK_DELAYABLE_DEFINE(cooldown_work, cooldown_expired);

// Callback function for button press
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    k_work_reschedule(&cooldown_work, K_MSEC(15));
}

// Button initialization function
int button_init(const struct gpio_dt_spec *button, struct gpio_callback *button_cb, button_event_handler_t handler)
{
    if (!handler) {
        return -EINVAL;
    }

    if (!device_is_ready(button->port)) {
        return -EIO;
    }

    int err = gpio_pin_configure_dt(button, GPIO_INPUT);
    if (err) {
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(button, GPIO_INT_EDGE_BOTH);
    if (err) {
        return err;
    }

    gpio_init_callback(button_cb, button_pressed, BIT(button->pin));
    err = gpio_add_callback(button->port, button_cb);
    if (err) {
        return err;
    }

    return 0;
}

// Helper function to handle button events
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
        return "Pressed";
        return "Released";
    default:
        return "Unknown";
    }
}

// User-defined button event handler implementation
static void button_event_handler(enum button_evt evt)
{
    printk("Button event: %s\n", helper_button_evt_str(evt));
}

// Main function
void main(void)
{
    printk("Button Debouncing Sample!\n");

    user_cb = button_event_handler;

    int err = button_init(&button0, &button0_cb_data, button_event_handler);
    if (err) {
        printk("Button0 Init failed: %d\n", err);
        return;
    }

    err = button_init(&button1, &button1_cb_data, button_event_handler);
    if (err) {
        printk("Button1 Init failed: %d\n", err);
        return;
    }

    if (!device_is_ready(led.port)) {
        printk("LED device not ready\n");
        return;
    }

    err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (err < 0) {
        printk("LED configuration failed: %d\n", err);
        return;
    }

    printk("Init succeeded. Waiting for event...\n");
}
