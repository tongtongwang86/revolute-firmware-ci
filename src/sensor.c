// sensor.c
#include "sensor.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/i2c.h>
#include <math.h>

#define M_PI 3.14159265358979323846
#define DEGREE_THRESHOLD 10.0f
#define RAD_THRESHOLD (DEGREE_THRESHOLD * (M_PI / 180.0f))

#define TLV493_NODE DT_NODELABEL(tlv493)
static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(TLV493_NODE);

static rotation_callback_t cw_callback = NULL;
static rotation_callback_t ccw_callback = NULL;


static int64_t last_max_sleep_time_ms = 0;


static float prev_cos = 0.0f;
static float prev_sin = 0.0f;

static float tmp_prev_cos = 0.0f;
static float tmp_prev_sin = 0.0f;
static bool first_vector = true;

static int sleep_ms = 10;
static int min_sleep_ms = 10;
static int max_sleep_ms = 100;


static int16_t extract_12bit(uint8_t msb, uint8_t lsb_part) {
    int16_t val = ((int16_t)msb << 4) | (lsb_part & 0x0F);
    if (val & 0x800) val |= 0xF000;
    return val;
}

static uint8_t calculate_parity(uint8_t reg0, uint8_t reg1, uint8_t reg2, uint8_t reg3) {
    uint8_t bits[4] = {reg0, reg1, reg2, reg3};
    uint8_t bit_sum = 0;
    for (int i = 0; i < 4; i++)
        for (int b = 0; b < 8; b++)
            bit_sum += (bits[i] >> b) & 1;
    return (bit_sum % 2 == 0) ? 1 : 0;
}

static void configure_tlv493_fast_mode(void) {
    uint8_t reg0, reg1, reg2, reg3;
    if (i2c_reg_read_byte_dt(&dev_i2c, 0x00, &reg0) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x01, &reg1) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x02, &reg2) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x03, &reg3) < 0) {
        printk("Failed to read config regs\n");
        return;
    }

    reg1 &= ~0x07;
    reg1 |= (1 << 1);  // FAST = 1
    uint8_t parity = calculate_parity(reg0, reg1, reg2, reg3);
    reg1 = (reg1 & 0x7F) | (parity << 7);

    i2c_reg_write_byte_dt(&dev_i2c, 0x01, reg1);
}

  void configure_tlv493_poweroff_mode(void) {
    uint8_t reg0 = 0, reg1 = 0, reg2 = 0, reg3 = 0;

    if (i2c_reg_read_byte_dt(&dev_i2c, 0x00, &reg0) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x01, &reg1) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x02, &reg2) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x03, &reg3) < 0) {
        printk("Failed to read configuration registers\n");
        return;
    }

    // Clear bits 2:0 (INT, FAST, LOW) to enter power-off mode
    reg1 &= ~0x07;

    // Recalculate parity
    uint8_t parity = calculate_parity(reg0, reg1, reg2, reg3);

    // Set parity bit (bit 7)
    reg1 = (reg1 & 0x7F) | (parity << 7);

    // Write modified MOD1 register
    int ret = i2c_reg_write_byte_dt(&dev_i2c, 0x01, reg1);
    if (ret < 0) {
        printk("Failed to write MOD1 register: %d\n", ret);
    } else {
        printk("Power-off mode configured, MOD1 updated.\n");
    }
}

 void configure_tlv493_low_power_mode(void) {
    uint8_t reg0 = 0, reg1 = 0, reg2 = 0, reg3 = 0;

    // Read registers 0x00 through 0x03
    if (i2c_reg_read_byte_dt(&dev_i2c, 0x00, &reg0) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x01, &reg1) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x02, &reg2) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x03, &reg3) < 0) {
        printk("Failed to read configuration registers\n");
        return;
    }

    // Clear bits 2:0 (INT, FAST, LOW)
    reg1 &= ~0x07;

    // Set INT = 0, FAST = 0, LOW = 1
    reg1 |= (1 << 0);  // LOW = 1

    // Calculate new parity over all 4 config bytes
    uint8_t parity = calculate_parity(reg0, reg1, reg2, reg3);

    // Set parity bit (bit 7)
    reg1 = (reg1 & 0x7F) | (parity << 7);

    // Write back only modified register (reg1 at address 0x01)
    int ret = i2c_reg_write_byte_dt(&dev_i2c, 0x01, reg1);
    if (ret < 0) {
        printk("Failed to write MOD1 register: %d\n", ret);
    } else {
        printk("Low-power mode configured, MOD1 updated with preserved bits.\n");
    }
}

 void tlv493_general_reset(void) {
    struct i2c_msg msg = {
        .buf = NULL,
        .len = 0,
        .flags = I2C_MSG_WRITE | I2C_MSG_STOP,
    };
    i2c_transfer(dev_i2c.bus, &msg, 1, 0x00);
    k_busy_wait(100);
}

static void track_rotation_direction(int16_t bx, int16_t by) {
    float mag = sqrtf((float)bx * bx + (float)by * by);
    if (mag < 1e-3f) return;

    float cos_curr = bx / mag;
    float sin_curr = by / mag;

    if (first_vector) {
        prev_cos = cos_curr;
        prev_sin = sin_curr;
        first_vector = false;
        return;
    }

    float delta_cos = prev_cos * cos_curr + prev_sin * sin_curr;
    float delta_sin = prev_cos * sin_curr - prev_sin * cos_curr;

    float angle_squared = delta_sin * delta_sin + (1.0f - delta_cos) * (1.0f - delta_cos);

    float tmp_delta_cos = tmp_prev_cos * cos_curr + tmp_prev_sin * sin_curr;
    float tmp_delta_sin = tmp_prev_cos * sin_curr - tmp_prev_sin * cos_curr;

    float tmp_angle_squared = tmp_delta_sin * tmp_delta_sin + (1.0f - tmp_delta_cos) * (1.0f - tmp_delta_cos);
    // float angle = sqrtf(angle_squared);  // rough angle delta in radians
    
        tmp_prev_cos = cos_curr;
        tmp_prev_sin = sin_curr;
        
    
// Use angle_squared / time to get rate (pseudo-angular velocity)
float velocity_metric = tmp_angle_squared / (float)sleep_ms;  // sleep_ms from previous iteration

printf("velocity: %.6f\n", velocity_metric);
// Define range for this "velocity" metric
// float min_velocity = 0.00001f;
// float max_velocity = 0.0001f;

// // Clamp
// if (velocity_metric < min_velocity) {
//     velocity_metric = min_velocity;
// } else if (velocity_metric > max_velocity) {
//     velocity_metric = max_velocity;
// }

// // Normalize
// float normalized = (velocity_metric - min_velocity) / (max_velocity - min_velocity);

// // Inverse mapping: high velocity → low sleep
// sleep_ms = (int)((1.0f - normalized) * (max_sleep_ms - min_sleep_ms) + min_sleep_ms);


    // Adjust polling delay: larger angle → shorter sleep
    if (velocity_metric > 0.000004f) {

        // float scale = 1.0f / (tmp_angle_squared * 100.0f);  // Tune factor as needed
        // if (scale < 1.0f) scale = 1.0f;
        // sleep_ms = (int)(max_sleep_ms / scale);
        // if (sleep_ms < min_sleep_ms) sleep_ms = min_sleep_ms;
        // if (sleep_ms > max_sleep_ms) sleep_ms = max_sleep_ms;

        sleep_ms = min_sleep_ms;
        last_max_sleep_time_ms = k_uptime_get();  // 🕒 Record time when sleep is maxed out
    }else{
        sleep_ms = max_sleep_ms;
        int64_t now = k_uptime_get();
        int64_t idle_duration = now - last_max_sleep_time_ms;
        if (idle_duration > 5000) {
            printk("System idle for over 5 seconds\n");
            // You could trigger a lower-power state, etc.
        }

    }

    if (angle_squared > RAD_THRESHOLD * RAD_THRESHOLD) {
        if (delta_sin > 0.0f && ccw_callback) ccw_callback();
        else if (delta_sin < 0.0f && cw_callback) cw_callback();

        prev_cos = cos_curr;
        prev_sin = sin_curr;
    }
}

static void i2c_scan_bus(const struct i2c_dt_spec *i2c) {
    printk("Starting I2C scan on bus %s...\n", i2c->bus->name);

    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        struct i2c_msg msgs[1];
        uint8_t dummy = 0;

        msgs[0].buf = &dummy;
        msgs[0].len = 1;
        msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

        int ret = i2c_transfer(i2c->bus, msgs, 1, addr);
        if (ret == 0) {
            printk("I2C device found at address 0x%02X\n", addr);
        }
    }

    printk("I2C scan complete.\n");
}


void sensor_read(void) {
    uint8_t raw[6];
    int ret = i2c_burst_read_dt(&dev_i2c, 0x00, raw, sizeof(raw));
    
    if (ret < 0) {
        printk("Failed to read sensor data: %d. Attempting reinitialization...\n", ret);
        tlv493_general_reset();
        configure_tlv493_fast_mode();
        ret = i2c_burst_read_dt(&dev_i2c, 0x00, raw, sizeof(raw));
        if (ret < 0) {
            printk("Retry failed: Sensor is still unresponsive.\n");
            return;
        } else {
            printk("Reinitialization succeeded.\n");
        }
    }

    int16_t bx = extract_12bit(raw[0], raw[4] >> 4);
    int16_t by = extract_12bit(raw[1], raw[4]);
    int16_t bz = extract_12bit(raw[2], raw[5]);

    printf("%.6f\n", sensor_get_strength(bx,by));
    
    if(sensor_get_strength(bx,by) > 50) {
        track_rotation_direction(bx, by);

                   // print sleep_ms 
    printk("%d \n", sleep_ms);


    k_msleep(sleep_ms);
    
    }else{

    k_msleep(1000);
    }



    
 
    
}


float sensor_get_strength(int16_t bx, int16_t by) {
    float fx = (float)bx;
    float fy = (float)by;
    return sqrtf(fx * fx + fy * fy);
}

void sensor_init(void) {
    if (!device_is_ready(dev_i2c.bus)) {
        printk("Sensor I2C bus not ready\n");
        return;
    }
    tlv493_general_reset();
    k_msleep(5);
    configure_tlv493_fast_mode();
    // configure_tlv493_poweroff_mode();
    
}

void register_cw_callback(rotation_callback_t cb) {
    cw_callback = cb;
}

void register_ccw_callback(rotation_callback_t cb) {
    ccw_callback = cb;
}
