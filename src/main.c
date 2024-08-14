#define M_PI   3.14159265358979323846264338327950288
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>
#include <math.h>  // Include math library for sqrt and atan2 functions

LOG_MODULE_REGISTER(Revolute, LOG_LEVEL_DBG);

static inline float out_ev(struct sensor_value *val)
{
    return (val->val1 + (float)val->val2 / 1000000);
}

static float gx = 0.0, gy = 0.0, gz = 0.0;
static const float FREQ = 12.5;  // Frequency in Hz

// Updated filter constants for more sensitivity and less smoothing
static const float ALPHA = 0.6;  // Higher accelerometer weight, reduced smoothing time
static const float Z_ALPHA = 0.7;  // Higher accelerometer weight for z-axis, increased sensitivity

static void fetch_and_display(const struct device *dev)
{
    struct sensor_value x, y, z;
    static int trig_cnt;
    static uint64_t last_time = 0;

    trig_cnt++;

    uint64_t current_time = k_uptime_get();
    float dT = (current_time - last_time) / 1000.0f;  // Time delta in seconds
    last_time = current_time;

    // lsm6dso accel
    sensor_sample_fetch_chan(dev, SENSOR_CHAN_ACCEL_XYZ);
    sensor_channel_get(dev, SENSOR_CHAN_ACCEL_X, &x);
    sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Y, &y);
    sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Z, &z);

    float accX = out_ev(&x);
    float accY = out_ev(&y);
    float accZ = out_ev(&z);

    // Calculate accelerometer-based angles
    float ax = atan2(accY, sqrt(pow(accX, 2) + pow(accZ, 2))) * 180.0 / M_PI;
    float ay = atan2(accX, sqrt(pow(accY, 2) + pow(accZ, 2))) * 180.0 / M_PI;

    // lsm6dso gyro
    sensor_sample_fetch_chan(dev, SENSOR_CHAN_GYRO_XYZ);
    sensor_channel_get(dev, SENSOR_CHAN_GYRO_X, &x);
    sensor_channel_get(dev, SENSOR_CHAN_GYRO_Y, &y);
    sensor_channel_get(dev, SENSOR_CHAN_GYRO_Z, &z);

    float gyrX = out_ev(&x);
    float gyrY = out_ev(&y);
    float gyrZ = out_ev(&z);

    // Gyroscope-based angles (integrated over time)
    gx += gyrX * dT;
    gy -= gyrY * dT;
    gz += gyrZ * dT;

    // Complementary filter with adjusted alpha for faster response and higher z-axis sensitivity
    gx = gx * (1.0 - ALPHA) + ax * ALPHA;
    gy = gy * (1.0 - ALPHA) + ay * ALPHA;
    gz = gz * (1.0 - Z_ALPHA) + gyrZ * Z_ALPHA;  // Increased sensitivity for z-axis

    // Display the filtered angles
    printf("x:%f,y:%f,z:%f\n", gx, gy, gz);
    printf("trig_cnt:%d\n\n", trig_cnt);
}

static int set_sampling_freq(const struct device *dev)
{
    int ret = 0;
    struct sensor_value odr_attr;

    // set accel/gyro sampling frequency to 12.5 Hz
    odr_attr.val1 = 12.5;
    odr_attr.val2 = 0;

    ret = sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ,
                          SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr);
    if (ret != 0) {
        printf("Cannot set sampling frequency for accelerometer.\n");
        return ret;
    }

    ret = sensor_attr_set(dev, SENSOR_CHAN_GYRO_XYZ,
                          SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr);
    if (ret != 0) {
        printf("Cannot set sampling frequency for gyro.\n");
        return ret;
    }

    return 0;
}

#ifdef CONFIG_LSM6DSO_TRIGGER
static void trigger_handler(const struct device *dev,
                            const struct sensor_trigger *trig)
{
    fetch_and_display(dev);
}

static void test_trigger_mode(const struct device *dev)
{
    struct sensor_trigger trig;

    if (set_sampling_freq(dev) != 0)
        return;

    trig.type = SENSOR_TRIG_DATA_READY;
    trig.chan = SENSOR_CHAN_ACCEL_XYZ;

    if (sensor_trigger_set(dev, &trig, trigger_handler) != 0) {
        printf("Could not set sensor type and channel\n");
        return;
    }
}

#else
static void test_polling_mode(const struct device *dev)
{
    if (set_sampling_freq(dev) != 0) {
        return;
    }

    while (1) {
        fetch_and_display(dev);
        k_sleep(K_MSEC((1000/FREQ)));  // Adjust delay to match frequency
    }
}
#endif

int main(void)
{
    const struct device *const dev = DEVICE_DT_GET_ONE(st_lsm6dso);

    if (!device_is_ready(dev)) {
        printk("%s: device not ready.\n", dev->name);
        return 0;
    }

#ifdef CONFIG_LSM6DSO_TRIGGER
    printf("Testing LSM6DSO sensor in trigger mode.\n\n");
    test_trigger_mode(dev);
#else
    printf("Testing LSM6DSO sensor in polling mode.\n\n");
    test_polling_mode(dev);
#endif
    return 0;
}
