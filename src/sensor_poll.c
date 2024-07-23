#include "sensor_poll.h"
#include "hog.h"
#include "revsvc.h"

LOG_MODULE_REGISTER(spin, LOG_LEVEL_DBG);

static struct k_thread sensorThread_data;
K_THREAD_STACK_DEFINE(sensorThread_stack_area, STACKSIZE);

double as5600_subdegree_refresh(const struct device *dev)
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

    double degrees = rot_raw.val1 + (float)rot_raw.val2 / 1000;

    return degrees;
}

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
    if (abs(delta_degree) < dead_zone)
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

void print_direction()
{
    if (direction == 1)
    {
        // clock wise
    }
    else if (direction == -1)
    {
        // counter clockwise
    }
    else
    {
        // below dead zone
    }
}

double last_degree = 0;
double last_time = 0;
double last_speed = 0;
int direction = 0;
double speed_threshold = 100;
double dead_zone = 2;
double continuous_counter = 0;

void spinUpdateThread(void)
{
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
    int last_ident = (as5600_refresh(as) - (as5600_refresh(as) % config.identPerRev)) / config.identPerRev;
    int8_t cappedValue;
    struct hid_revolute_report_body report;
    while (1)
    {
        new_degree = as5600_refresh(as);
        int degrees = (int)new_degree;
        int current_position = predictive_update(new_degree);
        change = (last_position - current_position);

        if (config.mode == 13 && (config.upReport[0] == 0 && config.downReport[0] == 0))
        {

            if (change > INT8_MAX)
            {
                cappedValue = INT8_MAX;
            }
            else if (change < INT8_MIN)
            {
                cappedValue = INT8_MIN;
            }
            else
            {
                cappedValue = (int8_t)change;
            }

            if (config.upReport[1] == 1 && config.downReport[1] == 1)
            {
                
                report.id = config.mode;
                report.report[0] = 0;
                report.report[1] = cappedValue;
                report.report[2] = 0;
                report.report[3] = 0;
                report.report[4] = 0;
                report.report[5] = 0;
                report.report[6] = 0;
                report.report[7] = 0;
            }
            else if (config.upReport[2] == 1 && config.downReport[2] == 1)
            {

                report.id = config.mode;
                report.report[0] = 0;
                report.report[1] = 0;
                report.report[2] = cappedValue;
                report.report[3] = 0;
                report.report[4] = 0;
                report.report[5] = 0;
                report.report[6] = 0;
                report.report[7] = 0;
            }
            else if (config.upReport[3] == 1 && config.downReport[3] == 1)
            {
                report.id = config.mode;
                report.report[0] = 0;
                report.report[1] = 0;
                report.report[2] = 0;
                report.report[3] = cappedValue;
                report.report[4] = 0;
                report.report[5] = 0;
                report.report[6] = 0;
                report.report[7] = 0;
            }
        }
        else
        {

            if (last_ident != (((degrees + (config.identPerRev / 2) + IDENT_OFFSET)) - ((degrees + (config.identPerRev / 2) + IDENT_OFFSET) % config.identPerRev)) / config.identPerRev &&
                (((degrees + (config.identPerRev / 2) + IDENT_OFFSET)) - ((degrees + (config.identPerRev / 2) + IDENT_OFFSET) % config.identPerRev)) / config.identPerRev != 30)
            {
                printk("tick");

                if (direction == 1)
                {
                    // clock wise
                    LOG_INF("tick cw");
                    report.id = config.mode;
                    report.report[0] = config.upReport[0];
                    report.report[1] = config.upReport[1];
                    report.report[2] = config.upReport[2];
                    report.report[3] = config.upReport[3];
                    report.report[4] = config.upReport[4];
                    report.report[5] = config.upReport[5];
                    report.report[6] = config.upReport[6];
                    report.report[7] = config.upReport[7];
                }
                else if (direction == -1)
                {
                    LOG_INF("tick ccw");
                    // counter clockwise
                    report.id = config.mode;
                    report.report[0] = config.downReport[0];
                    report.report[1] = config.downReport[1];
                    report.report[2] = config.downReport[2];
                    report.report[3] = config.downReport[3];
                    report.report[4] = config.downReport[4];
                    report.report[5] = config.downReport[5];
                    report.report[6] = config.downReport[6];
                    report.report[7] = config.downReport[7];
                }
                else
                {
                    // below dead zone
                
                }

                last_ident = ((degrees + (config.identPerRev / 2) + IDENT_OFFSET) - ((degrees + (config.identPerRev / 2) + IDENT_OFFSET) % config.identPerRev)) / config.identPerRev;
            }
        }

        if (k_cyc_to_ms_ceil64(k_cycle_get_32() - last_report_time) < 15)
        { // thing to do when not sending bluetooth
        }
        else
        { // send bluetooth at 60 hz
           
            

            if (config.mode == 13 && (config.upReport[0] == 0 && config.downReport[0] == 0))
        {

            revolute_bt_submit(&report);


        }else{
            revolute_bt_submit(&report);

                report.id = config.mode;
                    report.report[0] = 0;
                    report.report[1] = 0;
                    report.report[2] = 0;
                    report.report[3] = 0;
                    report.report[4] = 0;
                    report.report[5] = 0;
                    report.report[6] = 0;
                    report.report[7] = 0;
                    revolute_bt_submit(&report);

        }


            last_position = current_position;
            last_report_time = k_cycle_get_32();
        }
    }
}

void SensorThreadinit(void)
{
    k_thread_create(&sensorThread_data, sensorThread_stack_area,
                    K_THREAD_STACK_SIZEOF(sensorThread_stack_area),
                    spinUpdateThread, NULL, NULL, NULL,
                    PRIORITY, 0, K_FOREVER);
    k_thread_name_set(&sensorThread_data, "spinspinthread");

    k_thread_start(&sensorThread_data);
}
