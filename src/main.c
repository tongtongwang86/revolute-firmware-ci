#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

#include <zephyr/drivers/gpio.h>

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/services/dis.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/usb/class/usb_hid.h>


#include <hid_usage.h>
#include <hid_usage_pages.h>
#include <bluetooth/services/hids.h>

// magnetic angle sensor


#include <math.h>

LOG_MODULE_REGISTER(Revolute, LOG_LEVEL_DBG);


#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)
#define SW2_NODE DT_ALIAS(sw2)
#define SW3_NODE DT_ALIAS(sw3)
#define LED0_NODE DT_ALIAS(led0)



static const struct gpio_dt_spec leds[] = {
    GPIO_DT_SPEC_GET_OR(LED0_NODE, gpios, {0}),
};







double last_degree = 0;
double last_time = 0;
double last_speed = 0;
int direction = 0;             // 1 for clockwise, -1 for counterclockwise, 0 for no movement
double continuous_counter = 0; // Continuous counter for the wheel position
double speed_threshold = 100;  // Threshold for significant speed changes
double dead_zone = 2;          // Dead zone in degrees



int main(void)
{
    LOG_INF("initializing buttons");

    // Initialize buttons
    for (size_t i = 0; i < ARRAY_SIZE(buttons); i++)
    {
        int err = button_init(i, button_event_handler);
        if (err)
        {
            LOG_ERR("Button %zu Init failed: %d", i + 1, err);
            return err;
        }
    }

    LOG_INF("Init succeeded. Waiting for event...");



    const struct device *const as = DEVICE_DT_GET(DT_INST(0, ams_as5600));

    if (as == NULL || !device_is_ready(as))
    {
        printk("as5600 device tree not configured\n");
        return;
    }




    if (IS_ENABLED(CONFIG_SAMPLE_BT_USE_AUTHENTICATION))
    {
        bt_conn_auth_cb_register(&auth_cb_display);
        LOG_INF("Bluetooth authentication callbacks registered.\n");
    }

    k_work_init(&button_action_work, button_action_work_handler); // start button work queue

    LOG_INF("system started");

    int degrees = as5600_refresh(as);
    

    last_time = k_cycle_get_32();
    double new_degree = as5600_refresh(as);
    int last_position = predictive_update(new_degree);

    int start_time = k_cycle_get_32();
    int last_report_time = k_cycle_get_32();
    int sum = 0;
    int countr = 0;
    int change;
    int8_t cappedValue;

    while (1)
    {

        // Example usage
        double new_degree = as5600_refresh(as);
        read_value = new_degree;
        int current_position = predictive_update(new_degree);


            change = (last_position - current_position);

                if (change > INT8_MAX) {
        cappedValue = INT8_MAX;
    } else if (change < INT8_MIN) {
        cappedValue = INT8_MIN;
    } else {
        cappedValue = (int8_t)change;
    }


    

        if (k_cyc_to_ms_ceil64(k_cycle_get_32() - last_report_time) < 15)
        {

            int err = gpio_pin_set_dt(&leds[0], 0);

            if (err < 0)
            {
                // Handle the error
                return;
            }
        }
        else
        {


            


                 struct zmk_hid_mouse_report_body report = {
                    .report = {0, 0 , 0 , cappedValue , 0 , 0, 0, 0}
                };
            trigger_button(&report);


  

            
            last_position = current_position;

            last_report_time = k_cycle_get_32();




        }


        int elapsed_time = k_cycle_get_32() - start_time;
        uint64_t elapsed_ns = k_cyc_to_ns_ceil64(elapsed_time);



        start_time = k_cycle_get_32();
    }
    return 0;
}
