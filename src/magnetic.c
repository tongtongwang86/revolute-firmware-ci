#include "magnetic.h"
#include "revsvc.h"
#include "hog.h"

LOG_MODULE_REGISTER(spin, LOG_LEVEL_DBG);
static struct k_thread magnetic_thread_data;
static K_THREAD_STACK_DEFINE(magnetic_stack, THREAD_STACK_SIZE);

uint16_t angle = 0;

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

bool is_discrete(uint8_t transport, uint8_t report[8]) {
    switch (transport) {
        case 5: // keyboard
        case 9: // consumer
            return true;

        case 13: // mouse
            // Check if byte 2 or 3 of the report (index 1 or 2 in C) is nonzero
            return (report[1] == 0 || report[2] == 0);

        default:
            return true; // If transport type is unknown, assume discrete
    }
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
static void magnetic_thread(void *unused1, void *unused2, void *unused3)
{

    LOG_INF("magnetic thread started");

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

    while (1) {
        new_degree = as5600_refresh(as);
        int current_position = predictive_update(new_degree);
        change = (last_position - current_position);
        angle = (int)new_degree;


        if (change > 0)      // clock wise
            {
            if (is_discrete(config.up_transport, config.up_report))
                {

        int degrees = (int)new_degree;
        int DegreesPerIdent = 360/config.up_identPerRev;
        int adjustedDegrees = degrees + (DegreesPerIdent / 2) + IDENT_OFFSET;
        int CurrentIdent = (adjustedDegrees - (adjustedDegrees % DegreesPerIdent)) / DegreesPerIdent;


            if (last_ident != CurrentIdent && CurrentIdent != config.up_identPerRev)
            {   
                LOG_INF("tick cw");
                last_ident = CurrentIdent;
            }

                }else{// handle continuous




                }





            last_position = current_position;
            }
            else if (change < 0)  // counter clock wise
            {
            if (is_discrete(config.dn_transport, config.dn_report))
                {
        int degrees = (int)new_degree;
        int DegreesPerIdent = 360/config.up_identPerRev;
        int adjustedDegrees = degrees + (DegreesPerIdent / 2) + IDENT_OFFSET;
        int CurrentIdent = (adjustedDegrees - (adjustedDegrees % DegreesPerIdent)) / DegreesPerIdent;


            if (last_ident != CurrentIdent && CurrentIdent != config.up_identPerRev)
            {
                // tick
                LOG_INF("tick ccw");
                last_ident = CurrentIdent;
            }

                }else{// handle continuous




                }




            last_position = current_position;
            }
            else
            {
                // below dead zone
            }











        
    }
}




void SensorThreadinit(void)
{
    // Start the print thread
    k_thread_create(&magnetic_thread_data, magnetic_stack, K_THREAD_STACK_SIZEOF(magnetic_stack),
                    magnetic_thread, NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    return 0;
}
