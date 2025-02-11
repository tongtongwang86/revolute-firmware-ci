#include "button.h"
#include "ble.h"
#include "hog.h"
#include "led.h"
#include "pwmled.h"

LOG_MODULE_REGISTER(button, LOG_LEVEL_INF);

#define SW3_NODE DT_ALIAS(sw3)
static const struct gpio_dt_spec sw3 = GPIO_DT_SPEC_GET_OR(SW3_NODE, gpios, {0});

#define SINGLE_CLICK_TIMEOUT K_MSEC(400)
#define LONG_HOLD_THRESHOLD K_MSEC(700)
#define DEBOUNCE_TIME K_MSEC(50)

enum button_event {
    BUTTON_SINGLE_CLICK,
    BUTTON_DOUBLE_CLICK,
    BUTTON_TRIPLE_CLICK,
    BUTTON_LONG_HOLD,
};

static struct k_work_delayable button_work;
static struct k_work_delayable long_hold_work;
static struct k_work_delayable debounce_work;
static int click_count = 0;
static bool long_hold_detected = false;
static int64_t press_start_time = 0;
static int64_t last_press_time = 0;

void handle_button_event(enum button_event event) {
    switch (event) {
    case BUTTON_SINGLE_CLICK:
        LOG_INF("Single Click detected!");
        break;
    case BUTTON_DOUBLE_CLICK:
        LOG_INF("Double Click detected!");
        break;
    case BUTTON_TRIPLE_CLICK:
        LOG_INF("Triple Click detected!");
        remove_bonded_device();
        break;
    case BUTTON_LONG_HOLD:
        LOG_INF("Long Hold detected! Turning off");
        k_sleep(K_MSEC(2000));
        power_off();
        break;
    default:
        LOG_WRN("Unknown button event!");
        break;
    }
}

void button_work_handler(struct k_work *work) {
    if (long_hold_detected) {
        long_hold_detected = false;
    } else {
        if (click_count == 1) {
            handle_button_event(BUTTON_SINGLE_CLICK);
        } else if (click_count == 2) {
            handle_button_event(BUTTON_DOUBLE_CLICK);
        } else if (click_count == 3) {
            handle_button_event(BUTTON_TRIPLE_CLICK);
        }
    }
    click_count = 0;
}

void long_hold_handler(struct k_work *work) {
    long_hold_detected = true;
    handle_button_event(BUTTON_LONG_HOLD);
}

void debounce_handler(struct k_work *work) {
    int64_t now = k_uptime_get();
    if (gpio_pin_get_dt(&sw3)) {
        press_start_time = now;
        long_hold_detected = false;
        k_work_reschedule(&long_hold_work, LONG_HOLD_THRESHOLD);
    } else {
        int64_t press_duration = now - press_start_time;
        k_work_cancel_delayable(&long_hold_work);
        if (!long_hold_detected) {
            click_count++;
            k_work_reschedule(&button_work, SINGLE_CLICK_TIMEOUT);
        }
    }
}

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    int64_t now = k_uptime_get();
    if (now - last_press_time > k_ticks_to_ms_floor64(DEBOUNCE_TIME.ticks)) {
        last_press_time = now;
        k_work_reschedule(&debounce_work, DEBOUNCE_TIME);
    }
}

static struct gpio_callback button_cb_data;

void button_uninit(void) {
    k_work_cancel_delayable(&button_work);
    k_work_cancel_delayable(&long_hold_work);
    k_work_cancel_delayable(&debounce_work);
    gpio_remove_callback(sw3.port, &button_cb_data);
    gpio_pin_interrupt_configure_dt(&sw3, GPIO_INT_DISABLE);
    LOG_INF("Button uninitialized.");
}

void button_init(void) {
    if (!device_is_ready(sw3.port)) {
        LOG_ERR("Button device %s not ready", sw3.port->name);
        return;
    }
    button_uninit();
    int ret = gpio_pin_configure_dt(&sw3, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("Failed to configure button GPIO (err %d)", ret);
        return;
    }
    ret = gpio_pin_interrupt_configure_dt(&sw3, GPIO_INT_EDGE_BOTH);
    if (ret < 0) {
        LOG_ERR("Failed to configure button interrupt (err %d)", ret);
        return;
    }
    gpio_init_callback(&button_cb_data, button_pressed, BIT(sw3.pin));
    gpio_add_callback(sw3.port, &button_cb_data);
    k_work_init_delayable(&button_work, button_work_handler);
    k_work_init_delayable(&long_hold_work, long_hold_handler);
    k_work_init_delayable(&debounce_work, debounce_handler);
    LOG_INF("Button initialized.");
}

SYS_INIT(button_init, APPLICATION, 50);
