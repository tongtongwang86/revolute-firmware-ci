#define M_PI   3.14159265358979323846264338327950288
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <math.h>

LOG_MODULE_REGISTER(Revolute, LOG_LEVEL_DBG);

static inline float out_ev(struct sensor_value *val)
{
	return (val->val1 + (float)val->val2 / 1000000);
}

typedef struct {
    double x;
    double y;
    double z;
    double rx;
    double ry;
    double rz;
} sensorData;

typedef struct {
    double angle;       // Angle estimate
    double bias;        // Bias estimate
    double rate;        // Rate estimate
    double P[2][2];     // Error covariance matrix
} KalmanState;

static const double Q_angle = 0.001;
static const double Q_bias = 0.003;
static const double R_measure = 0.03;

static double dt = 100; // Time step (100 ms)

void initKalman(KalmanState *state, double initialAngle) {
    state->angle = initialAngle;
    state->bias = 0;
    state->P[0][0] = 0;
    state->P[0][1] = 0;
    state->P[1][0] = 0;
    state->P[1][1] = 0;
}

double kalmanFilter(KalmanState *state, double newRate, double newAngle) {
    // Predict step
    state->rate = newRate - state->bias;
    state->angle += dt * state->rate;

    // Update error covariance
    state->P[0][0] += dt * (dt * state->P[1][1] - state->P[0][1] - state->P[1][0] + Q_angle);
    state->P[0][1] -= dt * state->P[1][1];
    state->P[1][0] -= dt * state->P[1][1];
    state->P[1][1] += Q_bias * dt;

    // Calculate Kalman gain
    double S = state->P[0][0] + R_measure;
    double K[2];
    K[0] = state->P[0][0] / S;
    K[1] = state->P[1][0] / S;

    // Update estimate
    double y = newAngle - state->angle;
    state->angle += K[0] * y;
    state->bias += K[1] * y;

    // Update error covariance matrix
    double P00_temp = state->P[0][0];
    double P01_temp = state->P[0][1];

    state->P[0][0] -= K[0] * P00_temp;
    state->P[0][1] -= K[0] * P01_temp;
    state->P[1][0] -= K[1] * P00_temp;
    state->P[1][1] -= K[1] * P01_temp;

    return state->angle;
}

void calculateAnglesFromAccel(sensorData input, double *roll, double *pitch) {
    *roll = atan2(input.y, input.z) * 180 / M_PI;
    *pitch = atan2(-input.x, sqrt(input.y * input.y + input.z * input.z)) * 180 / M_PI;
}


sensorData getSensorData(const struct device *dev) {
    sensorData result;
    sensorData offset;
    offset.x = 0.160958;
    offset.y = 0.328879;
    offset.z = -0.028187;
    offset.rx = -0.000128;
    offset.ry = 0.004631;
    offset.rz = 0.001418;


    struct sensor_value x, y, z;

    sensor_sample_fetch_chan(dev, SENSOR_CHAN_ACCEL_XYZ);
	sensor_channel_get(dev, SENSOR_CHAN_ACCEL_X, &x);
	sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Y, &y);
	sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Z, &z);

    result.x = (double)out_ev(&x) + offset.x;
    result.y = (double)out_ev(&y) + offset.y;
    result.z = (double)out_ev(&z) + offset.z;

    sensor_sample_fetch_chan(dev, SENSOR_CHAN_GYRO_XYZ);
	sensor_channel_get(dev, SENSOR_CHAN_GYRO_X, &x);
	sensor_channel_get(dev, SENSOR_CHAN_GYRO_Y, &y);
	sensor_channel_get(dev, SENSOR_CHAN_GYRO_Z, &z);

    result.rx = (double)out_ev(&x) + offset.rx;
    result.ry = (double)out_ev(&y) + offset.ry;
    result.rz = (double)out_ev(&z) + offset.rz;
    return result;
}



static void getCalibrationResults(const struct device *dev){

    sensorData value = {0};
    struct sensor_value x, y, z;
    int NUM_ITERATIONS = 1000;
    for (int i = 0; i < NUM_ITERATIONS; i++) {

    sensor_sample_fetch_chan(dev, SENSOR_CHAN_ACCEL_XYZ);
	sensor_channel_get(dev, SENSOR_CHAN_ACCEL_X, &x);
	sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Y, &y);
	sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Z, &z);

    value.x += (double)out_ev(&x);
    value.y += (double)out_ev(&y);
    value.z += (double)out_ev(&z);

    sensor_sample_fetch_chan(dev, SENSOR_CHAN_GYRO_XYZ);
	sensor_channel_get(dev, SENSOR_CHAN_GYRO_X, &x);
	sensor_channel_get(dev, SENSOR_CHAN_GYRO_Y, &y);
	sensor_channel_get(dev, SENSOR_CHAN_GYRO_Z, &z);

    value.rx += (double)out_ev(&x);
    value.ry += (double)out_ev(&y);
    value.rz += (double)out_ev(&z);
    
    k_sleep(K_MSEC(5));

    }

    printf("x: %f, y: %f, z: %f, rx: %f, ry: %f, rz: %f\n",
           value.x / (double)NUM_ITERATIONS,
           value.y / (double)NUM_ITERATIONS,
           value.z / (double)NUM_ITERATIONS,
           value.rx / (double)NUM_ITERATIONS,
           value.ry / (double)NUM_ITERATIONS,
           value.rz / (double)NUM_ITERATIONS);

}

static void displayData(sensorData input){

    printf("x:%f,y:%f,z:%f,rx:%f,ry:%f,rz:%f,\n", input.x, input.y, input.z, input.rx, input.ry, input.rz);

}


    KalmanState rollState;
    KalmanState pitchState;


static void displayDataKalman(sensorData input) {
    double accelRoll, accelPitch;

    // Calculate angles from accelerometer data
    calculateAnglesFromAccel(input, &accelRoll, &accelPitch);

    // Integrate gyroscope data (convert from radians to degrees)
    double gyroRollRate = input.rx * 180 / M_PI;
    double gyroPitchRate = input.ry * 180 / M_PI;

    // Apply Kalman filter to combine gyro and accelerometer data
    double roll = kalmanFilter(&rollState, gyroRollRate, accelRoll);
    double pitch = kalmanFilter(&pitchState, gyroPitchRate, accelPitch);

    // Display the filtered roll and pitch angles
    printf("r:%f, p:%f\n", roll, pitch);
}


static int set_sampling_freq(const struct device *dev)
{
	int ret = 0;
	struct sensor_value odr_attr;

	/* set accel/gyro sampling frequency to 12.5 Hz */
	odr_attr.val1 = 12.5;
	odr_attr.val2 = 0;

	ret = sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ,
			SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr);
	if (ret != 0) {
		printk("Cannot set sampling frequency for accelerometer.\n");
		return ret;
	}

	ret = sensor_attr_set(dev, SENSOR_CHAN_GYRO_XYZ,
			SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr);
	if (ret != 0) {
		printk("Cannot set sampling frequency for gyro.\n");
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
		printk("Could not set sensor type and channel\n");
		return;
	}
}

#else
static void test_polling_mode(const struct device *dev)
{
	if (set_sampling_freq(dev) != 0) {
		return;
	}
    k_sleep(K_MSEC(1000));
    getCalibrationResults(dev);

	while (1) {
        sensorData value;
		value = getSensorData(dev);
        // displayData(value);
        displayDataKalman(value);
		k_sleep(K_MSEC(dt));
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
	printk("Testing LSM6DSO sensor in trigger mode.\n\n");
	test_trigger_mode(dev);
#else
	printk("Testing LSM6DSO sensor in polling mode.\n\n");
	test_polling_mode(dev);
#endif
	return 0;
}