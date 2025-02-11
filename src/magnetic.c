#include "magnetic.h"
#include "revsvc.h"
#include "hog.h"


#define AS5600_I2C_ADDR 0x36  // AS5600 I2C address (can be 0x36 or 0x37 depending on ADDR pin configuration)
#define AS5600_REG_STATUS 0x0B

#define MAGNET_NOT_DETECTED 0
#define MAGNET_DETECTED 2
#define MAGNET_TOO_WEAK 1
#define MAGNET_TOO_STRONG 3

#define AS5600_ANGLE_REGISTER_H 0x0E
#define AS5600_FULL_ANGLE       360
#define AS5600_PULSES_PER_REV   4096
#define AS5600_MILLION_UNIT     1000000

LOG_MODULE_REGISTER(spin, LOG_LEVEL_DBG);
static struct k_thread magnetic_thread_data;
static K_THREAD_STACK_DEFINE(magnetic_stack, THREAD_STACK_SIZE);

uint16_t angle = 0;
int change = 0;

struct as5600_dev_cfg {
    struct i2c_dt_spec i2c_port;
};

/* Device runtime data */
struct as5600_dev_data {
    uint16_t position; // Stores the current position in raw sensor units (0-4095)
};


int get_magnet_strength(const struct device *dev)
{
    int ret;
    uint8_t mag_status;
    uint8_t reg = AS5600_REG_STATUS;
    
    // Access the device's configuration from the dev structure
    const struct as5600_dev_cfg *dev_cfg = dev->config;

    // Write the register address to the AS5600, and then read the status byte
    ret = i2c_write_read(dev_cfg->i2c_port.bus, AS5600_I2C_ADDR, &reg, sizeof(reg), &mag_status, sizeof(mag_status));
    if (ret != 0) {
        printk("I2C read error: %d\n", ret);
        return ret;  // Return the error code from I2C read
    }

    int magnet_strength = MAGNET_NOT_DETECTED;  // Default is no magnet

    // Check if a magnet is detected (MD = 1)
    if (mag_status & 0x20) { // MD is bit 5
        magnet_strength = MAGNET_DETECTED;

        // Check if the magnet is too weak (ML = 1)
        if (mag_status & 0x10) { // ML is bit 4
            magnet_strength = MAGNET_TOO_WEAK;
        }
        // Check if the magnet is too strong (MH = 1)
        else if (mag_status & 0x08) { // MH is bit 3
            magnet_strength = MAGNET_TOO_STRONG;
        }
    }

    return magnet_strength;
}


int as5600_refresh(const struct device *dev)
{
    struct as5600_dev_data *dev_data = dev->data;
    const struct as5600_dev_cfg *dev_cfg = dev->config;

    uint8_t read_data[2] = {0, 0};
    uint8_t angle_reg = AS5600_ANGLE_REGISTER_H;

    // Perform I2C read using device tree I2C specification
    int err = i2c_write_read_dt(&dev_cfg->i2c_port,
                                &angle_reg,
                                1,
                                read_data,
                                sizeof(read_data));

    // If I2C read was successful, combine the high and low bytes to form the 12-bit angle
    if (!err) {
        dev_data->position = ((uint16_t)read_data[0] << 8) | read_data[1];
    }

    // Return the position as a scaled degree (0-360)
    int degree = (dev_data->position * AS5600_FULL_ANGLE) / AS5600_PULSES_PER_REV;
    return degree;
}


bool is_discrete(uint8_t transport, uint8_t report[8]) {
    switch (transport) {
        case 5: // keyboard
        case 9: // consumer
            return true;

        case 13: // mouse
            // Check if byte 2 or 3 of the report (index 1 or 2 in C) is nonzero
            return (report[2] == 0 && report[3] == 0 && report[4] == 0);

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
    change = 0;
    int last_ident = (as5600_refresh(as) - (as5600_refresh(as) % config.up_identPerRev)) / config.up_identPerRev;

    while (1) {
        new_degree = as5600_refresh(as);
        int current_position = predictive_update(new_degree);
        change = (last_position - current_position);
        angle = (int)new_degree;

        // LOG_INF("New degree: %d", (int)new_degree);
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
                
                revolute_up_submit();
                last_ident = CurrentIdent;
            }

                }else{// handle continuous
                revolute_up_cont_submit();
                k_msleep(5);
                

                }


            last_position = current_position;
            }
            else if (change < 0)  // counter clock wise
            {
            if (is_discrete(config.dn_transport, config.dn_report))
                {
        int degrees = (int)new_degree;
        int DegreesPerIdent = 360/config.dn_identPerRev;
        int adjustedDegrees = degrees + (DegreesPerIdent / 2) + IDENT_OFFSET;
        int CurrentIdent = (adjustedDegrees - (adjustedDegrees % DegreesPerIdent)) / DegreesPerIdent;


            if (last_ident != CurrentIdent && CurrentIdent != config.dn_identPerRev)
            {
                // tick
                LOG_INF("tick ccw");

                revolute_dn_submit();
                last_ident = CurrentIdent;
            }

                }else{// handle continuous

                revolute_dn_cont_submit();
                k_msleep(5);


                }




            last_position = current_position;
            }
            else
            {
                // below dead zone
            }


        
    }
}


void suspend_magnetic_thread(void)
{
    k_thread_suspend(&magnetic_thread_data);
    LOG_INF("Print thread suspended");
}

// Function to resume the print thread
void resume_magnetic_thread(void)
{
    k_thread_resume(&magnetic_thread_data);
    LOG_INF("Print thread resumed");
}

void SensorThreadinit(void)
{
    // Start the print thread
    k_thread_create(&magnetic_thread_data, magnetic_stack, K_THREAD_STACK_SIZEOF(magnetic_stack),
                    magnetic_thread, NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    return 0;
}

SYS_INIT(SensorThreadinit, APPLICATION, 50);