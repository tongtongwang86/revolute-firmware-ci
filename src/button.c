#include "button.h"
#include "ble.h"
#include "hog.h"
#include "led.h"
#include "pwmled.h"

LOG_MODULE_REGISTER(button, LOG_LEVEL_INF);

#define SW3_NODE DT_ALIAS(sw3)
static const struct gpio_dt_spec sw3 = GPIO_DT_SPEC_GET_OR(SW3_NODE, gpios, {0});

#define SINGLE_CLICK_TIMEOUT K_MSEC(400)   // Timeout for detecting multiple clicks
#define LONG_HOLD_THRESHOLD K_MSEC(700)  // Threshold for long hold

enum button_event {
    BUTTON_SINGLE_CLICK,
    BUTTON_DOUBLE_CLICK,
    BUTTON_TRIPLE_CLICK,
    BUTTON_LONG_HOLD,
};

static struct k_work_delayable button_work;
static struct k_work_delayable long_hold_work;
static int click_count = 0;
static bool long_hold_detected = false;
static int64_t press_start_time = 0;

void handle_button_event(enum button_event event) {
    switch (event) {
    case BUTTON_SINGLE_CLICK:
        LOG_INF("Single Click detected!");

        // int err = zmk_ble_set_device_name("newname");
        // if (err < 0) {
        //     LOG_ERR("Failed to set device name (err %d)", err);
        // }

        break;
    case BUTTON_DOUBLE_CLICK:
        LOG_INF("Double Click detected!");
       
            hog_send_mouse_button_2();
            
        break;
    case BUTTON_TRIPLE_CLICK:
        LOG_INF("Triple Click detected!");

        zmk_ble_clear_all_bonds();
        // target_state = STATE_PAIRING;
        break;
    case BUTTON_LONG_HOLD:
        LOG_INF("Long Hold detected!");

        target_state = STATE_OFF;
        LOG_INF("Turning off");
        k_sleep(K_MSEC(2000));
        power_off();
        // led_notify_trigger();
        break;
    default:
        LOG_WRN("Unknown button event!");
        break;
    }
}

void button_work_handler(struct k_work *work) {
    if (long_hold_detected) {
        // Do nothing, as the long hold has already been processed
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
    click_count = 0; // Reset click count
}

void long_hold_handler(struct k_work *work) {
    long_hold_detected = true;
    handle_button_event(BUTTON_LONG_HOLD);
}

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    int64_t now = k_uptime_get();

    if (gpio_pin_get_dt(&sw3)) {
        // Button press detected
        press_start_time = now;
        long_hold_detected = false; // Reset long hold detection
        k_work_reschedule(&long_hold_work, LONG_HOLD_THRESHOLD); // Schedule long hold check
    } else {
        // Button release detected
        int64_t press_duration = now - press_start_time;

        k_work_cancel_delayable(&long_hold_work); // Cancel long hold check if button released early

        if (!long_hold_detected) {
            // If long hold didn't trigger, count the press as a click
            click_count++;
            k_work_reschedule(&button_work, SINGLE_CLICK_TIMEOUT); // Process clicks after timeout
        }
    }
}



// GPIO Callback
static struct gpio_callback button_cb_data;

void button_uninit(void) {
    // Cancel any pending work items
    k_work_cancel_delayable(&button_work);
    k_work_cancel_delayable(&long_hold_work);

    // Remove the GPIO callback and disable interrupts
    gpio_remove_callback(sw3.port, &button_cb_data);
    int ret = gpio_pin_interrupt_configure_dt(&sw3, GPIO_INT_DISABLE);
    if (ret < 0) {
        LOG_ERR("Failed to disable button interrupt (err %d)", ret);
    }

    // // Optionally, you could also disable the GPIO pin if necessary
    // ret = gpio_pin_configure_dt(&sw3, GPIO_DISCONNECTED);
    // if (ret < 0) {
    //     LOG_ERR("Failed to disconnect button GPIO (err %d)", ret);
    // }

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

    LOG_INF("Button initialized.");
}
