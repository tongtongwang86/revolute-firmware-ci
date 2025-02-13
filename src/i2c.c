#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include "ble.h"
#include "hog.h"
#include "revsvc.h"
#include "batterylvl.h"
#include "button.h"
#include "settings.h"
#include "led.h"
#include "magnetic.h"

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

#define THREAD_STACK_SIZE 1024
#define THREAD_PRIORITY 5
uint16_t angle = 0;
#define MSGQ_SIZE 10
#define MSGQ_ALIGNMENT 4
K_MSGQ_DEFINE(angle_msgq, sizeof(int), MSGQ_SIZE, MSGQ_ALIGNMENT);

// Variables for predictive update
static double last_time = 0;
static double last_degree = 0;
static double last_speed = 0;
static double continuous_counter = 0;
static int direction = 0;  // 1: clockwise, -1: counterclockwise, 0: no movement


const struct device *const as = DEVICE_DT_GET(DT_INST(0, ams_as5600));
struct as5600_dev_cfg {
    struct i2c_dt_spec i2c_port;
};

/* Device runtime data */
struct as5600_dev_data {
    uint16_t position; // Stores the current position in raw sensor units (0-4095)
};

LOG_MODULE_REGISTER(i2ca, LOG_LEVEL_INF);


int get_magnet_strength(void)
{
    int ret;
    uint8_t mag_status;
    uint8_t reg = AS5600_REG_STATUS;
    
    // Access the device's configuration from the dev structure
    const struct as5600_dev_cfg *dev_cfg = as->config;

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
    LOG_INF("g");

    return magnet_strength;
}


int get_magnet_angle(void)
{
    struct as5600_dev_data *dev_data = as->data;
    const struct as5600_dev_cfg *dev_cfg = as->config;

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

// Predictive update function
int predictive_update(double new_degree)
{
    double current_time = k_cycle_get_32();  // Time in cycles
    double time_diff = current_time - last_time;

    // Calculate the current speed
    double delta_degree = new_degree - last_degree;

    // Adjust for wrapping of the angle (360 degrees wraparound)
    if (delta_degree > 180)
    {
        delta_degree -= 360;
    }
    else if (delta_degree < -180)
    {
        delta_degree += 360;
    }

    // Ignore minor changes within the dead zone
    if (fabs(delta_degree) < DEADZONE)
    {
        direction = 0;  // No significant movement
        return (int)continuous_counter;
    }

    double current_speed = delta_degree / time_diff;  // Degrees per cycle

    // Detect direction change and significant speed changes
    int new_direction = (current_speed > 0) ? 1 : (current_speed < 0) ? -1 : 0;
    if (new_direction != direction || fabs(current_speed) > SPEED_THRESHOLD)
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


void angular_velocity_handler(struct k_work *work)
{
    static int last_angle = 0;
    static uint32_t last_time = 0;
    
    int current_angle = 0;
    uint32_t current_time = k_uptime_get_32();  // Get current time in ms

    // Read current angle from the sensor
    current_angle = get_magnet_angle();

    // Calculate angular velocity: (change in angle) / (time elapsed)
    if (last_time != 0) {
        int delta_angle = current_angle - last_angle;
        uint32_t delta_time = current_time - last_time;
        
        if (delta_time > 0) {
            int angular_velocity = delta_angle * 1000 / delta_time;  // Convert to degrees per second

            printk("Angular Velocity: %d degrees/sec\n", angular_velocity);
        }
    }

    // Update last angle and time for the next calculation
    last_angle = current_angle;
    last_time = current_time;
}
K_WORK_DEFINE(angular_velocity_work, angular_velocity_handler);


void sensor_thread(void)
{
    if (as == NULL || !device_is_ready(as))
    {
        printk("AS5600 device not configured\n");
        return;
    }

    while (1) {
        int angle = get_magnet_angle();

        // Send the angle to the message queue
        k_msgq_put(&angle_msgq, &angle, K_NO_WAIT);

        // Post work to calculate angular velocity
        k_work_submit(&angular_velocity_work);

        k_msleep(10);  // Sleep for 10 ms
    }
}


/* Global variable for the thread object */
static struct k_thread sensor_thread_data;
static K_THREAD_STACK_DEFINE(sensor_thread_stack, THREAD_STACK_SIZE);

void start_sensor_thread(void)
{
    k_thread_create(&sensor_thread_data, sensor_thread_stack, THREAD_STACK_SIZE,
                    sensor_thread, NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);
}


// SYS_INIT(start_sensor_thread, APPLICATION, 50);