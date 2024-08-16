#define M_PI   3.14159265358979323846264338327950288
#include "ble.h"
#include "revsvc.h"
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
    double roll;
    double pitch;
    double yaw;
} orientation;

// Define the quaternion struct
typedef struct quaternion {
    float q1; // Real component
    float q2; // i component
    float q3; // j component
    float q4; // k component
} quaternion;

// Global variables
quaternion q_est = { 1, 0, 0, 0}; // Initialize with the unit vector with real component = 1

// Define constants
const float BETA = 0.1f;  // Madgwick filter gain
const float DELTA_T = 0.1f;  // Sampling time in seconds (100 ms)

// Quaternion operations
quaternion quat_mult(quaternion L, quaternion R) {
    quaternion product;
    product.q1 = (L.q1 * R.q1) - (L.q2 * R.q2) - (L.q3 * R.q3) - (L.q4 * R.q4);
    product.q2 = (L.q1 * R.q2) + (L.q2 * R.q1) + (L.q3 * R.q4) - (L.q4 * R.q3);
    product.q3 = (L.q1 * R.q3) - (L.q2 * R.q4) + (L.q3 * R.q1) + (L.q4 * R.q2);
    product.q4 = (L.q1 * R.q4) + (L.q2 * R.q3) - (L.q3 * R.q2) + (L.q4 * R.q1);
    return product;
}

void quat_scalar(quaternion *q, float scalar) {
    q->q1 *= scalar;
    q->q2 *= scalar;
    q->q3 *= scalar;
    q->q4 *= scalar;
}

void quat_add(quaternion *result, quaternion q1, quaternion q2) {
    result->q1 = q1.q1 + q2.q1;
    result->q2 = q1.q2 + q2.q2;
    result->q3 = q1.q3 + q2.q3;
    result->q4 = q1.q4 + q2.q4;
}

void quat_sub(quaternion *result, quaternion q1, quaternion q2) {
    result->q1 = q1.q1 - q2.q1;
    result->q2 = q1.q2 - q2.q2;
    result->q3 = q1.q3 - q2.q3;
    result->q4 = q1.q4 - q2.q4;
}

void quat_normalization(quaternion *q) {
    float norm = sqrt(q->q1 * q->q1 + q->q2 * q->q2 + q->q3 * q->q3 + q->q4 * q->q4);
    if (norm > 0) {
        q->q1 /= norm;
        q->q2 /= norm;
        q->q3 /= norm;
        q->q4 /= norm;
    }
}

// Madgwick Filter Implementation
void imu_filter(float ax, float ay, float az, float gx, float gy, float gz) {
    quaternion q_est_prev = q_est;
    quaternion q_est_dot = {0}; // Placeholder in equations 42 and 43
    quaternion q_a = {0, ax, ay, az}; // Raw acceleration values, needs to be normalized
    quaternion q_w;

    // Integrate angular velocity to obtain position in angles
    q_w.q1 = 0;
    q_w.q2 = gx;
    q_w.q3 = gy;
    q_w.q4 = gz;

    quat_scalar(&q_w, 0.5);  // equation (12) dq/dt = (1/2)q*w
    q_w = quat_mult(q_est_prev, q_w);  // equation (12)

    // Normalize the acceleration quaternion to be a unit quaternion
    quat_normalization(&q_a);

    // Compute the gradient
    float F_g[3] = {
        2 * (q_est_prev.q2 * q_est_prev.q4 - q_est_prev.q1 * q_est_prev.q3) - q_a.q2,
        2 * (q_est_prev.q1 * q_est_prev.q2 + q_est_prev.q3 * q_est_prev.q4) - q_a.q3,
        2 * (0.5 - q_est_prev.q2 * q_est_prev.q2 - q_est_prev.q3 * q_est_prev.q3) - q_a.q4
    };

    float J_g[3][4] = {
        {-2 * q_est_prev.q3, 2 * q_est_prev.q4, -2 * q_est_prev.q1, 2 * q_est_prev.q2},
        {2 * q_est_prev.q2, 2 * q_est_prev.q1, 2 * q_est_prev.q4, 2 * q_est_prev.q3},
        {0, -4 * q_est_prev.q2, -4 * q_est_prev.q3, 0}
    };

    quaternion gradient = {
        J_g[0][0] * F_g[0] + J_g[1][0] * F_g[1] + J_g[2][0] * F_g[2],
        J_g[0][1] * F_g[0] + J_g[1][1] * F_g[1] + J_g[2][1] * F_g[2],
        J_g[0][2] * F_g[0] + J_g[1][2] * F_g[1] + J_g[2][2] * F_g[2],
        J_g[0][3] * F_g[0] + J_g[1][3] * F_g[1] + J_g[2][3] * F_g[2]
    };

    // Normalize the gradient
    quat_normalization(&gradient);

    // Sensor fusion
    quat_scalar(&gradient, BETA); // Multiply normalized gradient by beta
    quat_sub(&q_est_dot, q_w, gradient); // Subtract above from q_w
    quat_scalar(&q_est_dot, DELTA_T);
    quat_add(&q_est, q_est_prev, q_est_dot); // Integrate orientation rate to find position
    quat_normalization(&q_est); // Normalize the orientation of the estimate
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

static void displayMadgwickFilter(sensorData input) {
    // Update the Madgwick filter with the current sensor data
    imu_filter(input.x, input.y, input.z, input.rx, input.ry, input.rz);

    // Compute the Euler angles from the quaternion
    // float roll, pitch, yaw;
    // eulerAngles(q_est, &roll, &pitch, &yaw);

    // Print the roll, pitch, and yaw
    // printf("roll: %f, pitch: %f, yaw: %f\n", roll, pitch, yaw);
    // printf("x:%f,y:%f,z:%f\n", roll, pitch, yaw);q_est.q1
    printf("r:%f,i:%f,j:%f,k:%f\n", q_est.q1, q_est.q2, q_est.q3,q_est.q4);
}

static void displayData(sensorData input){


    printf("x:%f,y:%f,z:%f,rx:%f,ry:%f,rz:%f,\n", input.x, input.y, input.z, input.rx, input.ry, input.rz);

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
        displayMadgwickFilter(value);
        // displayData(value);
        int err = rev_send_gyro(q_est.q1, q_est.q2, q_est.q3,q_est.q4);
        printk("%d\n",err);
		k_sleep(K_SECONDS(DELTA_T));
	}
}



int main(void)
{
    enableBle();
    startAdv();

	const struct device *const dev = DEVICE_DT_GET_ONE(st_lsm6dso);

	if (!device_is_ready(dev)) {
		printk("%s: device not ready.\n", dev->name);
		return 0;
	}


	printk("Testing LSM6DSO sensor in polling mode.\n\n");
	test_polling_mode(dev);
	return 0;
}