#include "magnetic.h"
#include "revsvc.h"
#include "hog.h"

LOG_MODULE_REGISTER(spin, LOG_LEVEL_DBG);
static struct k_thread print_thread_data;
static K_THREAD_STACK_DEFINE(print_stack, THREAD_STACK_SIZE);


int as5600_refresh(const struct device *dev)
{
    int ret;
    struct sensor_value rot_raw;
    ret = sensor_sample_fetch_chan(dev, SENSOR_CHAN_ROTATION);
    if (ret != 0)
    {
        printk("Sample fetch error: %d\n", ret);
        return ret;
    }
    ret = sensor_channel_get(dev, SENSOR_CHAN_ROTATION, &rot_raw);
    if (ret != 0)
    {
        printk("Sensor channel get error: %d\n", ret);
        return ret;
    }

    int degrees = rot_raw.val1;

    return degrees;
}

int predictive_update(double new_degree)
{
    double current_time = k_cycle_get_32();
    double time_diff = current_time - last_time;

    // Calculate the current speed
    double delta_degree = new_degree - last_degree;
    if (delta_degree > 180)
    {
        delta_degree -= 360;
    }
    else if (delta_degree < -180)
    {
        delta_degree += 360;
    }

    // Ignore minor changes within the dead zone
    if (abs(delta_degree) < config.deadzone)
    {
        direction = 0;
        return (int)continuous_counter;
    }

    double current_speed = delta_degree / time_diff;

    // Detect direction change and significant speed changes
    int new_direction = (current_speed > 0) ? 1 : (current_speed < 0) ? -1
                                                                      : 0;
    if (new_direction != direction || abs(current_speed) > speed_threshold)
    {
        // Adjust speed calculation on direction change or significant speed change
        last_speed = current_speed;
        direction = new_direction;
    }
    else
    {
        // Smooth speed calculation using weighted average
        last_speed = (last_speed * 0.7) + (current_speed * 0.3);
    }

    // Update the continuous counter
    continuous_counter += delta_degree;

    // Update the last known values
    last_degree = new_degree;
    last_time = current_time;

    return (int)continuous_counter;
}


// Thread entry point
static void print_thread(void *unused1, void *unused2, void *unused3)
{

    LOG_INF("Print thread started");

    const struct device *const as = DEVICE_DT_GET(DT_INST(0, ams_as5600));

    if (as == NULL || !device_is_ready(as))
    {
        printk("as5600 device tree not configured\n");
        return;
    }

    double new_degree = as5600_refresh(as);
    int last_position = predictive_update(new_degree);
    int last_report_time = k_cycle_get_32();
    int change = 0;
    int last_ident = (as5600_refresh(as) - (as5600_refresh(as) % config.up_identPerRev)) / config.up_identPerRev;
    int8_t cappedValue;
    while (1) {
        new_degree = as5600_refresh(as);

        int degrees = (int)new_degree;
        int current_position = predictive_update(new_degree);
        change = (last_position - current_position);

 int DegreesPerIdent = 360/config.up_identPerRev;
    int adjustedDegrees = degrees + (DegreesPerIdent / 2) + IDENT_OFFSET;
    int CurrentIdent = (adjustedDegrees - (adjustedDegrees % DegreesPerIdent)) / DegreesPerIdent;


            if (last_ident != CurrentIdent && CurrentIdent != config.up_identPerRev)
            {


                if (change > 0)
                {
                    // clock wise
                    LOG_INF("tick cw");

                    // hog_send_mouse_button_1();
                    

                    last_position = current_position;
                }
                else if (change < 0)
                {
                    LOG_INF("tick ccw");
                //   hog_send_mouse_button_1();

                    last_position = current_position;
                }
                else
                {
                    // below dead zone
                }

                last_ident = CurrentIdent;
            }
        



    }
}




void SensorThreadinit(void)
{
    // Start the print thread
    k_thread_create(&print_thread_data, print_stack, K_THREAD_STACK_SIZEOF(print_stack),
                    print_thread, NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    return 0;
}
