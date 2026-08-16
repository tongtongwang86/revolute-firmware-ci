#include "sensor.h"
#include "statemanager.h"
#include "hog.h"
#include "power.h"
#include "ble.h"
#include "batterylvl.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <math.h>

LOG_MODULE_REGISTER(sensor, LOG_LEVEL_INF);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG_TO_RAD(d) ((float)(d) * ((float)M_PI / 180.0f))

/* Rotation the shaft must travel before one detent is reported, per direction.
 * Derived from the configured idents-per-revolution; kept in radians because
 * that is what the tracker works in. */
static float cw_step_rad = DEG_TO_RAD(12.0f);
static float ccw_step_rad = DEG_TO_RAD(12.0f);

/* Minimum step, so a nonsense configuration cannot make the wheel chatter. */
#define MIN_STEP_DEG 1.0f

/* Field magnitude below which we treat the magnet as absent. */
#define MAGNET_PRESENT_THRESHOLD 50.0f

/* Idle time before the device drops into hold mode. */
#define HOLD_IDLE_MS 5000

/* Poll periods for each run state. */
#define POLL_ACTIVE_MS  10
#define POLL_HOLD_MS    100
#define POLL_STANDBY_MS 2000

/* How often the battery level is refreshed while the bus is up. */
#define BATTERY_INTERVAL_MS 2000

/* Set to 1 to get per-sample register dumps on the console. This is a
 * diagnostic only: dumping registers costs ten extra I2C reads per sample and
 * makes it impossible to keep up with a fast spin. */
#define SENSOR_DEBUG_DUMP 0

float degreeThreshold = 10.0f;
float DEADZONE = DEG_TO_RAD(10.0f);

/* Kept for the existing external API. */
float CW_IDENT = DEG_TO_RAD(12.0f);
float CCW_IDENT = DEG_TO_RAD(12.0f);

static float step_from_idents(uint8_t idents_per_rev)
{
    float degrees = (idents_per_rev > 0) ? (360.0f / (float)idents_per_rev) : 12.0f;

    /* The configured deadzone raises the floor: it is the smallest movement
     * that should ever count as intentional. */
    float floor_deg = MAX((float)config.deadzone, MIN_STEP_DEG);

    if (degrees < floor_deg) {
        degrees = floor_deg;
    }

    return DEG_TO_RAD(degrees);
}

void sensor_set_degree_threshold(float threshold) {
    degreeThreshold = threshold;
    DEADZONE = DEG_TO_RAD(threshold);
}

/* These two used to write the same shared `degreeThreshold`, so whichever ran
 * last decided the step for both directions. Each direction now keeps its own. */
void set_cw_identsperrev(void) {
    cw_step_rad = step_from_idents(config.up_identPerRev);
    CW_IDENT = cw_step_rad;
}

void set_ccw_identsperrev(void) {
    ccw_step_rad = step_from_idents(config.dn_identPerRev);
    CCW_IDENT = ccw_step_rad;
}

#define TLV493_NODE DT_NODELABEL(tlv493)
static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(TLV493_NODE);

/* Rotation tracker state. The sensor gives a field vector, not an absolute
 * angle, so rotation is integrated from the signed angle between consecutive
 * samples. */
static float prev_cos = 0.0f;
static float prev_sin = 0.0f;
static bool first_vector = true;

/* Rotation accumulated since the last reported detent, signed: positive is
 * clockwise. */
static float angle_accum = 0.0f;

/* Fractional degrees accumulated for continuous (relative) reports. */
static float cont_accum = 0.0f;

static int64_t last_motion_ms;
static int64_t last_activity_reschedule_ms;
static bool magnet_present;

/* Cleared to ask the polling thread to exit at the next opportunity. */
static volatile bool sensor_running;
static struct k_sem sensor_stopped;

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

#if SENSOR_DEBUG_DUMP
static void dump_tlv493_registers(void)
{
    uint8_t regs[10];
    for (int i = 0; i < 10; i++) {
        if (i2c_reg_read_byte_dt(&dev_i2c, i, &regs[i]) < 0) {
            printk("Reg 0x%02X read failed\n", i);
            return;
        }
    }
    printk("TLV493D registers:\n");
    for (int i = 0; i < 10; i++) {
        printk("  Reg%02X = 0x%02X\n", i, regs[i]);
    }
}
#endif

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
    uint8_t reg0, reg1, reg2, reg3;

    if (i2c_reg_read_byte_dt(&dev_i2c, 0x00, &reg0) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x01, &reg1) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x02, &reg2) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x03, &reg3) < 0) {
        printk("Failed to read configuration registers\n");
        return;
    }

    // Clear FAST (bit1) and LOW (bit0) only
    reg1 &= ~0x03;

    // Recalculate parity across bytes 0-3
    uint8_t parity = calculate_parity(reg0, reg1, reg2, reg3);

    // Update parity bit (bit7)
    reg1 = (reg1 & 0x7F) | (parity << 7);

    // Write back updated MOD1 register
    int ret = i2c_reg_write_byte_dt(&dev_i2c, 0x01, reg1);
    if (ret < 0) {
        printk("Failed to write MOD1 register: %d\n", ret);
    } else {
        printk("Sensor set to Power-down mode.\n");
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

float sensor_get_strength(int16_t bx, int16_t by) {
    float fx = (float)bx;
    float fy = (float)by;
    return sqrtf(fx * fx + fy * fy);
}

/* Integrate the field vector into detents.
 *
 * The signed angle between consecutive samples is taken directly with atan2f,
 * which gives a real per-sample rotation instead of a squared-distance metric.
 * That matters for two reasons: the metric was unsigned, so the clockwise and
 * counter-clockwise branches ended up testing against different thresholds and
 * the two directions did not behave the same; and having a true angle means a
 * sample that spans several detents can emit all of them instead of one.
 */
static void track_rotation(int16_t bx, int16_t by)
{
    float mag = sqrtf((float)bx * bx + (float)by * by);

    if (mag < 1e-3f) {
        return;
    }

    float cos_curr = bx / mag;
    float sin_curr = by / mag;

    if (first_vector) {
        prev_cos = cos_curr;
        prev_sin = sin_curr;
        first_vector = false;
        return;
    }

    /* Rotation from the previous sample to this one, signed, in radians.
     * Unambiguous as long as the shaft moves less than half a turn between
     * samples; at the 100 Hz active rate that is ~30000 RPM of headroom. */
    float delta_cos = prev_cos * cos_curr + prev_sin * sin_curr;
    float delta_sin = prev_cos * sin_curr - prev_sin * cos_curr;
    float theta = atan2f(delta_sin, delta_cos);

    prev_cos = cos_curr;
    prev_sin = sin_curr;

    angle_accum += theta;
    cont_accum += theta * (180.0f / (float)M_PI);

    bool emitted = false;

    if (is_discrete(config.up_transport, config.up_report)) {
        /* Loop rather than branch: a fast spin can cross several detents inside
         * one sample period and every one of them is a real click. */
        while (angle_accum >= cw_step_rad) {
            revolute_up_submit();
            angle_accum -= cw_step_rad;
            emitted = true;
        }
    }

    if (is_discrete(config.dn_transport, config.dn_report)) {
        while (angle_accum <= -ccw_step_rad) {
            revolute_dn_submit();
            angle_accum += ccw_step_rad;
            emitted = true;
        }
    }

    /* Continuous (relative) transports report accumulated whole degrees. */
    if (!is_discrete(config.up_transport, config.up_report) ||
        !is_discrete(config.dn_transport, config.dn_report)) {
        int steps = (int)cont_accum;

        if (steps != 0) {
            cont_accum -= (float)steps;
            steps = CLAMP(steps, -127, 127);

            if (steps > 0 && !is_discrete(config.up_transport, config.up_report)) {
                revolute_up_cont_submit((int8_t)steps);
                emitted = true;
            } else if (steps < 0 && !is_discrete(config.dn_transport, config.dn_report)) {
                revolute_dn_cont_submit((int8_t)steps);
                emitted = true;
            }
        }
    }

    if (emitted) {
        int64_t now = k_uptime_get();

        last_motion_ms = now;

        /* Rescheduling on every single tick would hammer the workqueue at a few
         * hundred calls a second; once every few seconds is enough to keep the
         * inactivity timer from expiring during use. */
        if (now - last_activity_reschedule_ms > 5000) {
            last_activity_reschedule_ms = now;
            power_off_timer_reschedule();
        }
    }
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
            magnet_present = false;
            return;
        }
    }

    int16_t bx = extract_12bit(raw[0], raw[4] >> 4);
    int16_t by = extract_12bit(raw[1], raw[4]);

#if SENSOR_DEBUG_DUMP
    int16_t bz = extract_12bit(raw[2], raw[5]);

    printk("bx=%d by=%d bz=%d\n", bx, by, bz);
    dump_tlv493_registers();
#endif

    magnet_present = sensor_get_strength(bx, by) > MAGNET_PRESENT_THRESHOLD;

    if (magnet_present) {
        track_rotation(bx, by);
    }
}

void sensor_reinit_after_resume(void)
{
    /* The sensor lost its configuration along with its supply, and the stored
     * reference vector describes a rotation that happened before the gap. */
    tlv493_general_reset();
    k_msleep(5);
    configure_tlv493_fast_mode();

    first_vector = true;
    angle_accum = 0.0f;
    cont_accum = 0.0f;
}

// ======= Polling thread =======

#define SENSOR_THREAD_STACK_SIZE 1536
#define SENSOR_THREAD_PRIORITY 5

K_THREAD_STACK_DEFINE(sensor_thread_stack, SENSOR_THREAD_STACK_SIZE);
static struct k_thread sensor_thread_data;

static void refresh_battery_if_due(int64_t *last_battery_ms)
{
    int64_t now = k_uptime_get();

    if (now - *last_battery_ms < BATTERY_INTERVAL_MS) {
        return;
    }

    *last_battery_ms = now;

    /* Battery reads share the bus with the sensor, so they are done from this
     * thread while the bus is known to be up rather than from a free-running
     * thread that could hit a suspended bus during standby. One fetch serves
     * both the empty check and the notification. */
    int percent = battery_read_percent();

    if (battery_level_is_empty(percent)) {
        LOG_WRN("Battery empty, powering off");
        /* Deferred to the workqueue: power_off() stops this thread. */
        schedule_power_off(0);
        return;
    }

    if (advertising_status == ADV_NONE) {
        battery_publish(percent);
    }
}

static void sensor_thread_fn(void *arg1, void *arg2, void *arg3) {
    ARG_UNUSED(arg1); ARG_UNUSED(arg2); ARG_UNUSED(arg3);

    int64_t last_battery_ms = k_uptime_get();

    last_motion_ms = k_uptime_get();

    while (sensor_running) {
        int64_t start = k_uptime_get();
        int poll_ms;

        switch (power_status) {
        case PWR_STANDBY:
            /* Bus and sensor rail are down. Bring them up just long enough to
             * check whether the magnet is back. */
            power_resume();
            sensor_read();
            refresh_battery_if_due(&last_battery_ms);

            if (magnet_present && advertising_status == ADV_NONE) {
                power_status = PWR_ON;
                onhold = false;
                last_motion_ms = k_uptime_get();
                poll_ms = POLL_ACTIVE_MS;
            } else {
                power_standby();
                poll_ms = POLL_STANDBY_MS;
            }
            break;

        case PWR_HOLD:
            sensor_read();
            refresh_battery_if_due(&last_battery_ms);

            if (!magnet_present || advertising_status != ADV_NONE) {
                power_status = PWR_STANDBY;
                onhold = false;
                power_standby();
                poll_ms = POLL_STANDBY_MS;
            } else if (k_uptime_get() - last_motion_ms < HOLD_IDLE_MS) {
                /* Movement while holding: back to the fast rate. */
                power_status = PWR_ON;
                onhold = false;
                poll_ms = POLL_ACTIVE_MS;
            } else {
                poll_ms = POLL_HOLD_MS;
            }
            break;

        case PWR_ON:
        default:
            sensor_read();
            refresh_battery_if_due(&last_battery_ms);

            if (!magnet_present || advertising_status != ADV_NONE) {
                power_status = PWR_STANDBY;
                onhold = false;
                power_standby();
                poll_ms = POLL_STANDBY_MS;
            } else if (k_uptime_get() - last_motion_ms > HOLD_IDLE_MS) {
                /* Nothing has moved for a while. Drop the sample rate and let
                 * the LED show that the wheel is parked; any movement brings
                 * the device straight back. */
                power_status = PWR_HOLD;
                onhold = true;
                poll_ms = POLL_HOLD_MS;
            } else {
                poll_ms = POLL_ACTIVE_MS;
            }
            break;
        }

        int64_t elapsed = k_uptime_get() - start;
        int64_t sleep_time = poll_ms - elapsed;

        if (sleep_time > 0) {
            k_msleep(sleep_time);
        } else if (power_status == PWR_ON) {
            LOG_WRN("Sensor read overrun: %lld ms", elapsed);
            k_yield();
        }
    }

    k_sem_give(&sensor_stopped);
}

// ======= Initialization =======

void sensor_init(void) {
    k_sem_init(&sensor_stopped, 0, 1);

    if (!device_is_ready(dev_i2c.bus)) {
        printk("Sensor I2C bus not ready\n");
        return;
    }

    tlv493_general_reset();
    k_msleep(5);
    configure_tlv493_fast_mode();

    set_cw_identsperrev();
    set_ccw_identsperrev();

    sensor_running = true;

    k_thread_create(&sensor_thread_data, sensor_thread_stack,
                    K_THREAD_STACK_SIZEOF(sensor_thread_stack),
                    sensor_thread_fn,
                    NULL, NULL, NULL,
                    SENSOR_THREAD_PRIORITY, 0, K_NO_WAIT);
}

void sensor_stop(void) {
    /* Ask the thread to finish its current sample rather than aborting it
     * mid-transfer and leaving the bus in an undefined state. */
    if (sensor_running) {
        sensor_running = false;
        k_sem_take(&sensor_stopped, K_MSEC(500));
    }

    configure_tlv493_poweroff_mode();
}
