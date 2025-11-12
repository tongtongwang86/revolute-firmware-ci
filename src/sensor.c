#include "sensor.h"
#include "statemanager.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/i2c.h>
#include <math.h>
#include "haptic.h"

#define M_PI 3.14159265358979323846
// #define DEGREE_THRESHOLD 10.0f
// #define DEADZONE (DEGREE_THRESHOLD * (M_PI / 180.0f))

float degreeThreshold = 10.0f;
float DEADZONE = 10.0f * (M_PI / 180.0f);

float CW_IDENT = 50.0f * (M_PI / 180.0f);
float CCW_IDENT = 50.0f * (M_PI / 180.0f);

void sensor_set_degree_threshold(float threshold) {
    degreeThreshold = threshold;
    DEADZONE = degreeThreshold * (M_PI / 180.0f);
}

void set_cw_identsperrev() {
    degreeThreshold = 360/config.up_identPerRev;
    CW_IDENT = degreeThreshold * (M_PI / 180.0f);
}


void set_ccw_identsperrev() {
    
    degreeThreshold = 360/config.dn_identPerRev;
    CCW_IDENT = degreeThreshold * (M_PI / 180.0f);
}

#define TLV493_NODE DT_NODELABEL(tlv493)
static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(TLV493_NODE);

static float prev_cos = 0.0f;
static float prev_sin = 0.0f;
static float tmp_prev_cos = 0.0f;
static float tmp_prev_sin = 0.0f;
static bool first_vector = true;

static int64_t last_max_sleep_time_ms = 0;
static int sleep_ms = 10;
static const int min_sleep_ms = 10;
static const int max_sleep_ms = 100;


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

    reg1 &= ~0x07;
    uint8_t parity = calculate_parity(reg0, reg1, reg2, reg3);
    reg1 = (reg1 & 0x7F) | (parity << 7);
    int ret = i2c_reg_write_byte_dt(&dev_i2c, 0x01, reg1);
    if (ret < 0) {
        printk("Failed to write MOD1 register: %d\n", ret);
    } else {
        printk("Power-off mode configured, MOD1 updated.\n");
    }
}

void configure_tlv493_low_power_mode(void) {
    uint8_t reg0 = 0, reg1 = 0, reg2 = 0, reg3 = 0;
    if (i2c_reg_read_byte_dt(&dev_i2c, 0x00, &reg0) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x01, &reg1) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x02, &reg2) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x03, &reg3) < 0) {
        printk("Failed to read configuration registers\n");
        return;
    }

    reg1 &= ~0x07;
    reg1 |= (1 << 0);  // LOW = 1
    uint8_t parity = calculate_parity(reg0, reg1, reg2, reg3);
    reg1 = (reg1 & 0x7F) | (parity << 7);
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

// static void track_rotation_direction(int16_t bx, int16_t by) {
//     float mag = sqrtf((float)bx * bx + (float)by * by);
//     if (mag < 1e-3f) return;

//     float cos_curr = bx / mag;
//     float sin_curr = by / mag;

//     if (first_vector) {
//         prev_cos = cos_curr;
//         prev_sin = sin_curr;
//         first_vector = false;
//         return;
//     }

//     float delta_cos = prev_cos * cos_curr + prev_sin * sin_curr;
//     float delta_sin = prev_cos * sin_curr - prev_sin * cos_curr;
//     float angle_squared = delta_sin * delta_sin + (1.0f - delta_cos) * (1.0f - delta_cos);

//     float tmp_delta_cos = tmp_prev_cos * cos_curr + tmp_prev_sin * sin_curr;
//     float tmp_delta_sin = tmp_prev_cos * sin_curr - tmp_prev_sin * cos_curr;
//     float tmp_angle_squared = tmp_delta_sin * tmp_delta_sin + (1.0f - tmp_delta_cos) * (1.0f - tmp_delta_cos);
//     tmp_prev_cos = cos_curr;
//     tmp_prev_sin = sin_curr;

//     float velocity_metric = tmp_angle_squared / (float)sleep_ms;

//     if (velocity_metric > 0.000005f) {
//         sleep_ms = min_sleep_ms;
//         last_max_sleep_time_ms = k_uptime_get();
//     } else {
//         sleep_ms = max_sleep_ms;
//         int64_t now = k_uptime_get();
//         int64_t idle_duration = now - last_max_sleep_time_ms;
//         if (idle_duration > 5000) {
//             printk("System idle for over 5 seconds\n");
//             onhold = true;
//         }else{
//             onhold = false;

//         }
//     }

//     if (angle_squared > DEADZONE * DEADZONE) {
//         if (delta_sin > 0.0f) {
//             printk("cw\n");
//         } else if (delta_sin < 0.0f) {
//             printk("ccw\n");
//         }
//         prev_cos = cos_curr;
//         prev_sin = sin_curr;
//     }
// }


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
    tmp_prev_cos = cos_curr;
    tmp_prev_sin = sin_curr;

    float velocity_metric = tmp_angle_squared ;

    // if (velocity_metric > 0.000005f) {
    //     sleep_ms = min_sleep_ms;
    //     last_max_sleep_time_ms = k_uptime_get();
    // } else {
    //     sleep_ms = max_sleep_ms;
    //     int64_t now = k_uptime_get();
    //     int64_t idle_duration = now - last_max_sleep_time_ms;
    //     if (idle_duration > 5000) {
    //         printk("System idle for over 5 seconds\n");
    //         onhold = true;
    //     }else{
    //         onhold = false;

    //     }
    // }

    if (angle_squared > DEADZONE * DEADZONE) {
        
        sleep_ms = min_sleep_ms;

        if (delta_sin > 0.0f){
            if (is_discrete(config.up_transport, config.up_report)){
                    revolute_up_submit();
                    // led4_pulse_submit();
                    printk("cw\n");
                    prev_cos = cos_curr;
                    prev_sin = sin_curr;
                    last_max_sleep_time_ms = k_uptime_get();
                
            }else{
                    
                    printf(" %f\n", velocity_metric);
                    prev_cos = cos_curr;
                    prev_sin = sin_curr;
                    last_max_sleep_time_ms = k_uptime_get();

            }


        }else if (delta_sin < 0.0f) {
            if (is_discrete(config.dn_transport, config.dn_report)){
                if(angle_squared > CCW_IDENT * CCW_IDENT){
                    revolute_dn_submit();
                    // led4_pulse_submit();
                    printk("ccw\n");
                    prev_cos = cos_curr;
                    prev_sin = sin_curr;
                    last_max_sleep_time_ms = k_uptime_get();

                }
                
            }else{
                    printf("%f\n", velocity_metric);
                    prev_cos = cos_curr;
                    prev_sin = sin_curr;
                    last_max_sleep_time_ms = k_uptime_get();

            }


        }


        // if (delta_sin > 0.0f && (angle_squared > CW_IDENT * CW_IDENT)) {


        //     printk("cw\n");
        //       prev_cos = cos_curr;
        // prev_sin = sin_curr;
        //     last_max_sleep_time_ms = k_uptime_get();
        // } else if (delta_sin < 0.0f && (angle_squared > CCW_IDENT * CCW_IDENT)) {

        //     printk("ccw\n");
        //       prev_cos = cos_curr;
        // prev_sin = sin_curr;
        //     last_max_sleep_time_ms = k_uptime_get();
        // } 
      
    }

        sleep_ms = max_sleep_ms;
         int64_t now = k_uptime_get();
        int64_t idle_duration = now - last_max_sleep_time_ms;
        if (idle_duration > 5000) {
            printk("System idle for over 5 seconds\n");
            onhold = true;
        }else{
            onhold = false;

        }


    
}

float sensor_get_strength(int16_t bx, int16_t by) {
    float fx = (float)bx;
    float fy = (float)by;
    return sqrtf(fx * fx + fy * fy);
}

void sensor_read(void) {
    uint8_t raw[6];
    int ret = i2c_burst_read_dt(&dev_i2c, 0x00, raw, sizeof(raw));
    if (ret < 0) {
        printk("Failed to read sensor data: %d. Attempting reinit...\n", ret);
        i2c_scan_bus(&dev_i2c);
        tlv493_general_reset();
        configure_tlv493_fast_mode();
        ret = i2c_burst_read_dt(&dev_i2c, 0x00, raw, sizeof(raw));
        if (ret < 0) {
            printk("Retry failed: Sensor still unresponsive.\n");
            return;
        }
    }

    int16_t bx = extract_12bit(raw[0], raw[4] >> 4);
    int16_t by = extract_12bit(raw[1], raw[4]);
    int16_t bz = extract_12bit(raw[2], raw[5]);

    if (sensor_get_strength(bx, by) > 50) {
        track_rotation_direction(bx, by);
    }
}

// ======= 100 Hz polling thread setup =======

#define SENSOR_THREAD_STACK_SIZE 1024
#define SENSOR_THREAD_PRIORITY 5
#define SENSOR_POLL_PERIOD_MS 10  // 100 Hz

K_THREAD_STACK_DEFINE(sensor_thread_stack, SENSOR_THREAD_STACK_SIZE);
static struct k_thread sensor_thread_data;

static void sensor_thread_fn(void *arg1, void *arg2, void *arg3) {
    ARG_UNUSED(arg1); ARG_UNUSED(arg2); ARG_UNUSED(arg3);
    while (1) {
        int64_t start = k_uptime_get();
        sensor_read();
        // printk("sleep_ms: %d\n", sleep_ms);


        int64_t elapsed = k_uptime_get() - start;
        int64_t sleep_time = SENSOR_POLL_PERIOD_MS - elapsed;
        if (sleep_time > 0) {
            k_msleep(sleep_time);
        } else {
            printk("Sensor read overrun: %lld ms\n", elapsed);
        }
    }
}

// ======= Initialization =======

void sensor_init(void) {
    if (!device_is_ready(dev_i2c.bus)) {
        printk("Sensor I2C bus not ready\n");
        return;
    }

    

    tlv493_general_reset();
    k_msleep(5);
    configure_tlv493_fast_mode();

    set_cw_identsperrev();
    set_ccw_identsperrev();

    k_thread_create(&sensor_thread_data, sensor_thread_stack,
                    K_THREAD_STACK_SIZEOF(sensor_thread_stack),
                    sensor_thread_fn,
                    NULL, NULL, NULL,
                    SENSOR_THREAD_PRIORITY, 0, K_NO_WAIT);
}

void sensor_stop(void) {
    /* Abort the sensor polling thread to ensure it releases I2C and stops activity */
    k_thread_abort(&sensor_thread_data);
}
