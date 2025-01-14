
#include "button.h"
// thread stuff

k_tid_t button_thread_tid;

LOG_MODULE_REGISTER(button, LOG_LEVEL_DBG);

#define SW3_NODE DT_ALIAS(sw3)
static const struct gpio_dt_spec sw3 = GPIO_DT_SPEC_GET(SW3_NODE, gpios);

void button_thread_fn(void *arg1, void *arg2, void *arg3) {
    int ret;

    if (!device_is_ready(sw3.port)) {
        LOG_ERR("Button device %s not ready", sw3.port->name);
        return;
    }

    ret = gpio_pin_configure_dt(&sw3, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("Failed to configure button GPIO (err %d)", ret);
        return;
    }

    // Enable pull-up or pull-down (if needed)
    ret = gpio_pin_interrupt_configure_dt(&sw3, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure button interrupt (err %d)", ret);
        return;
    }

    LOG_INF("Button thread started");

    while (1) {
        if (gpio_pin_get_dt(&sw3)) {
            LOG_INF("Button pressed! Initiating Bluetooth unpair...");
            bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);

            // Debounce delay
            k_sleep(K_MSEC(500));
            // k_work_submit(&advertise_acceptlist_work);
        }

        k_sleep(K_MSEC(100));  // Polling delay
    }
}


// Thread entry function for hog_button_loop
void hog_button_thread(void *arg1, void *arg2, void *arg3) {
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    hog_button_loop();
}



void rev_button_thread_init(void) {

      button_thread_tid = k_thread_create(&rev_button_thread_data, rev_button_stack,
                    K_THREAD_STACK_SIZEOF(rev_button_stack),
                    button_thread_fn,NULL, NULL, NULL,
                    REV_BUTTON_THREAD_PRIORITY, 0, K_NO_WAIT);


    

    LOG_INF("rev_button_loop thread initialized");
}