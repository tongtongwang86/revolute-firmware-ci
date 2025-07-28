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

static float prev_cos = 0.0f;
static float prev_sin = 0.0f;
static float tmp_prev_cos = 0.0f;
static float tmp_prev_sin = 0.0f;
static bool first_vector = true;

static int64_t last_max_sleep_time_ms = 0;
static int sleep_ms = 10;
static const int min_sleep_ms = 10;
static const int max_sleep_ms = 100;

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

    float velocity_metric = tmp_angle_squared / (float)sleep_ms;

    if (velocity_metric > 0.000004f) {
        sleep_ms = min_sleep_ms;
        last_max_sleep_time_ms = k_uptime_get();
    } else {
        sleep_ms = max_sleep_ms;
        int64_t now = k_uptime_get();
        int64_t idle_duration = now - last_max_sleep_time_ms;
        if (idle_duration > 5000) {
            printk("System idle for over 5 seconds\n");
        }
    }

    if (angle_squared > RAD_THRESHOLD * RAD_THRESHOLD) {
        if (delta_sin > 0.0f) {
            printk("cw\n");
        } else if (delta_sin < 0.0f) {
            printk("ccw\n");
        }
        prev_cos = cos_curr;
        prev_sin = sin_curr;
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

    k_thread_create(&sensor_thread_data, sensor_thread_stack,
                    K_THREAD_STACK_SIZEOF(sensor_thread_stack),
                    sensor_thread_fn,
                    NULL, NULL, NULL,
                    SENSOR_THREAD_PRIORITY, 0, K_NO_WAIT);
}
