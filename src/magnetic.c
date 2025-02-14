#include "magnetic.h"
#include "revsvc.h"
#include "hog.h"
#include "power.h"
#include "batterylvl.h"

int last_ident;
int last_position;
double new_degree;

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

uint16_t CW_IDENT_OFFSET ; // angle offset in degrees
uint16_t CCW_IDENT_OFFSET ; // angle offset in degrees

LOG_MODULE_REGISTER(spin, LOG_LEVEL_DBG);
static struct k_thread magnetic_thread_data;
static K_THREAD_STACK_DEFINE(magnetic_stack, THREAD_STACK_SIZE);

const struct device *const as = DEVICE_DT_GET(DT_INST(0, ams_as5600));

// K_MUTEX_DEFINE(i2c_mutex);

uint16_t angle = 0;
int change = 0;

uint8_t magnet_strength = 0;

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
        printk("Magnet detected\n");
        // Check if the magnet is too weak (ML = 1)
        if (mag_status & 0x10) { // ML is bit 4
            magnet_strength = MAGNET_TOO_WEAK;
            printk("Magnet too weak\n");

        }
        // Check if the magnet is too strong (MH = 1)
        else if (mag_status & 0x08) { // MH is bit 3
            magnet_strength = MAGNET_TOO_STRONG;
            printk("Magnet too strong\n");
        }
    }else{
        printk("Magnet not detected\n");

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

void calculate_and_send(void){

       
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
    int adjustedDegrees = degrees + (DegreesPerIdent / 2) + CW_IDENT_OFFSET;
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
        int adjustedDegrees = degrees + (DegreesPerIdent / 2) + CCW_IDENT_OFFSET;
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


void calculate_and_sendnew(void){

       
    new_degree = as5600_refresh(as);

    int current_position = predictive_update(new_degree);
    change = (last_position - current_position);
    angle = (int)new_degree;
    // LOG_INF("New degree: %d", (int)new_degree % (360/config.up_identPerRev));

     
    if (change > 0)      // clock wise
        {
       

    int degrees = (int)new_degree;
    int DegreesPerIdent = 360/config.up_identPerRev;
    int adjustedDegrees = degrees + (DegreesPerIdent / 2) + CW_IDENT_OFFSET;
    // LOG_INF("adjustedDegrees: %d", CCW_IDENT_OFFSET);
    int CurrentIdent = (adjustedDegrees - (adjustedDegrees % DegreesPerIdent)) / DegreesPerIdent;


        if (last_ident != CurrentIdent && CurrentIdent != 0)
        {   
            LOG_INF("tick cw");
            LOG_INF("CurrentIdent: %d", CurrentIdent);
            LOG_INF("CurrentIdent: %d", degrees);
            revolute_up_submit();
            last_ident = CurrentIdent;
        }

            


        last_position = current_position;
        }
        else if (change < 0)  // counter clock wise
        {
      
        int degrees = (int)new_degree;
        int DegreesPerIdent = 360/config.dn_identPerRev;
        int adjustedDegrees = degrees + (DegreesPerIdent / 2) + CCW_IDENT_OFFSET;
        // LOG_INF("adjustedDegrees: %d", CCW_IDENT_OFFSET); 
        int CurrentIdent = (adjustedDegrees - (adjustedDegrees % DegreesPerIdent)) / DegreesPerIdent;


        if (last_ident != CurrentIdent && CurrentIdent != 0)
        {
            // tick
            LOG_INF("tick ccw");
            LOG_INF("CurrentIdent: %d", CurrentIdent);
            LOG_INF("CurrentIdent: %d", degrees);
            revolute_dn_submit();
            last_ident = CurrentIdent;
        }

            



        last_position = current_position;
        }
        else
        {
            // below dead zone
        }
    
    
    
}

struct angle_data {
    uint16_t last_angle;      // Store the angle as a float (or appropriate type)
    uint32_t timestamp;    // Store the timestamp in milliseconds or another unit
};

struct angle_data last_angle_data = {0.0, 0};



bool check_no_movement (void){

    if (change == 0 && last_angle_data.last_angle != new_degree){   //no change and last angle is not equal to new angle
        last_angle_data.last_angle = new_degree;
        last_angle_data.timestamp = k_uptime_get_32();
        return false;
    }else if (change == 0 && last_angle_data.last_angle == new_degree){ // no change and last angle is equal to new angle
        // no movement detected
        if (k_uptime_get_32() - last_angle_data.timestamp > 5000){
            CW_IDENT_OFFSET = (int)new_degree % (360/config.up_identPerRev); // angle offset in degrees
            CCW_IDENT_OFFSET = (int)new_degree % (360/config.dn_identPerRev); // angle offset in degrees

            // LOG_INF("CW_IDENT_OFFSET: %d", CW_IDENT_OFFSET);
            // LOG_INF("CCW_IDENT_OFFSET: %d", CCW_IDENT_OFFSET);
            return true;
        }else{

            return false;
        }

    }else{ // movement detected
        return false;

    }
    
  
}


// Thread entry point
static void magnetic_thread(void *unused1, void *unused2, void *unused3)
{

    LOG_INF("magnetic thread started");


    if (as == NULL || !device_is_ready(as))
    {
        printk("as5600 device tree not configured\n");
        return;
    }

    new_degree = as5600_refresh(as);
    last_position = predictive_update(new_degree);
    // int lasttime = k_cycle_get_32();
    change = 0;
    last_ident = (as5600_refresh(as) - (as5600_refresh(as) % config.up_identPerRev)) / config.up_identPerRev;

    double lasttime = k_cycle_get_32();
    // double lasttime2 = 0;
    while (1) {
        
        double time = k_cycle_get_32();
        uint32_t elapsed_ms = k_ticks_to_ms_floor32(time - lasttime);
        // uint32_t elapsed_ms2 = k_ticks_to_ms_floor32(time - lasttime);
        // printk("Elapsed time: %d", elapsed_ms);

        // if (elapsed_ms % 10 == 0){
        //     LOG_INF("10ms");
        //     if (check_no_movement()){
        //         LOG_INF("no movement detected");
        //         power_status = PWR_HOLD;
        //         power_standby();
        //     }

        //     lasttime2 = time;
        // }

       

            switch (power_status) {
                case PWR_ON:
                if (elapsed_ms > 2000) {
            
                
                    if (is_battery_empty()) {
                        power_off();
                    } else if (advertising_status == ADV_NONE) {
                        sendbattery();
                    }
            
                    if (!get_magnet_strength(as)) {
                        power_status = PWR_STANDBY;
                        power_standby();
                        k_msleep(2000);
                    }else if (check_no_movement()){
                        power_status = PWR_HOLD;
                        power_standby();
                        k_msleep(10);


                    }
                    lasttime = time;
                }
                    break;
            
                case PWR_STANDBY:
                    power_resume();
                    k_msleep(10);
            
                    if (is_battery_empty()) {
                        power_off();
                    } else if (advertising_status == ADV_NONE) {
                        sendbattery();
                    }
            
                    if (get_magnet_strength(as)) {
                        power_status = PWR_ON;
                    } else {
                        power_standby();
                        k_msleep(2000);
                    }
                    break;
                case PWR_HOLD:
                power_resume();
                k_msleep(10);
                
                if (is_battery_empty()) {
                    power_off();
                } else if (advertising_status == ADV_NONE) {
                    sendbattery();
                }

                new_degree = as5600_refresh(as);
                int current_position = predictive_update(new_degree);
                change = (last_position - current_position);

                if (!check_no_movement()) {
                    power_status = PWR_ON;
                } else {
                    power_standby();
                    k_msleep(10);
                }
                break;
            
                default:
                    // Handle other power status cases if any (e.g., error state)
                    break;
            }
            

           
    

                // lasttime = time;
        // }
  
        

        if(power_status == PWR_ON){
            calculate_and_send();
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

SYS_INIT(SensorThreadinit, APPLICATION, 50);