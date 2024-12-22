#define M_PI   3.14159265358979323846264338327950288
#include "ble.h"
#include "revsvc.h"
#include "hog.h"
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/time_units.h>
#include <math.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(Revolute, LOG_LEVEL_DBG);

bool notify_mysensor_enabled = 0;

static const struct gpio_dt_spec sw3 = GPIO_DT_SPEC_GET(DT_ALIAS(sw3), gpios);

static const struct gpio_dt_spec sw2 = GPIO_DT_SPEC_GET(DT_ALIAS(sw2), gpios);

static inline float out_ev(struct sensor_value *val)
{
	return (val->val1 + (float)val->val2 / 1000000);
}

typedef struct {
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

float DELTA_T = 0; 
float start_cycles, end_cycles, cycle_diff;



quaternion quaternionMultiply(quaternion q, quaternion p) {
    quaternion result;
    result.q1 = q.q1 * p.q1 - q.q2 * p.q2 - q.q3 * p.q3 - q.q4 * p.q4;
    result.q2 = q.q1 * p.q2 + q.q2 * p.q1 + q.q3 * p.q4 - q.q4 * p.q3;
    result.q3 = q.q1 * p.q3 - q.q2 * p.q4 + q.q3 * p.q1 + q.q4 * p.q2;
    result.q4 = q.q1 * p.q4 + q.q2 * p.q3 - q.q3 * p.q2 + q.q4 * p.q1;
    return result;
}

// Normalize a quaternion
void normalizeQuaternion(quaternion *q) {
    float norm = sqrt(q->q1 * q->q1 + q->q2 * q->q2 + q->q3 * q->q3 + q->q4 * q->q4);
    q->q1 /= norm;
    q->q2 /= norm;
    q->q3 /= norm;
    q->q4 /= norm;
}

void rotationToQuaternion(float rx, float ry, float rz) {
    // Time step (assume DELTA_T is global and updated externally)
    extern float DELTA_T;

    // Convert angular velocity to a quaternion
    quaternion omega_q = {0, rx, ry, rz};

    // Compute quaternion derivative: 0.5 * q_est * omega_q
    quaternion q_dot = quaternionMultiply(q_est, omega_q);
    q_dot.q1 *= 0.5f;
    q_dot.q2 *= 0.5f;
    q_dot.q3 *= 0.5f;
    q_dot.q4 *= 0.5f;

    // Integrate to update q_est
    q_est.q1 += q_dot.q1 * DELTA_T;
    q_est.q2 += q_dot.q2 * DELTA_T;
    q_est.q3 += q_dot.q3 * DELTA_T;
    q_est.q4 += q_dot.q4 * DELTA_T;

    // Normalize q_est to avoid numerical errors
    normalizeQuaternion(&q_est);
}



sensorData getSensorData(const struct device *dev) {
    sensorData result;
    sensorData offset;

    offset.rx = -0.000128;
    offset.ry = 0.004631;
    offset.rz = 0.001418;


    struct sensor_value x, y, z;


    sensor_sample_fetch_chan(dev, SENSOR_CHAN_GYRO_XYZ);
	sensor_channel_get(dev, SENSOR_CHAN_GYRO_X, &x);
	sensor_channel_get(dev, SENSOR_CHAN_GYRO_Y, &y);
	sensor_channel_get(dev, SENSOR_CHAN_GYRO_Z, &z);

    result.rx = (double)out_ev(&x) + offset.rx;
    result.ry = (double)out_ev(&y) + offset.ry;
    result.rz = (double)out_ev(&z) + offset.rz;
    return result;
}




static int set_sampling_freq(const struct device *dev)
{
	int ret = 0;
	struct sensor_value odr_attr;

	/* set accel/gyro sampling frequency to 12.5 Hz */
	odr_attr.val1 = 300;
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









static void poll_sensor(const struct device *dev)
{
	if (set_sampling_freq(dev) != 0) {
		return;
	}

	while (1) {
        sensorData value;


        cycle_diff = k_cycle_get_32() - start_cycles;
        DELTA_T = (float)k_cyc_to_ms_floor32(cycle_diff)/1000;

		value = getSensorData(dev);
        start_cycles = k_cycle_get_32();
        
        k_sleep(K_MSEC(1));

        rotationToQuaternion( value.rx, value.ry, value.rz);
        printf("dt:%f,rx:%f,ry:%f,rz:%f",DELTA_T, value.rx, value.ry, value.rz);

    printf("r:%f,i:%f,j:%f,k:%f\n", q_est.q1, q_est.q2, q_est.q3,q_est.q4);

if (notify_mysensor_enabled == 1){

         int err = rev_send_gyro(q_est.q1, q_est.q2, q_est.q3,q_est.q4);

         if (err){

printk("send error: %d\n",err);
         }
        
}


 if (!device_is_ready(sw3.port)) {
        printk("Error: sw3 is not ready\n");
        return;
    }

    int ret = gpio_pin_configure_dt(&sw3, GPIO_INPUT);
    if (ret != 0) {
        printk("Failed to configure sw3 pin\n");
        return;
    }

 if (!device_is_ready(sw2.port)) {
        printk("Error: sw2 is not ready\n");
        return;
    }

    ret = gpio_pin_configure_dt(&sw2, GPIO_INPUT);
    if (ret != 0) {
        printk("Failed to configure sw2 pin\n");
        return;
    }




if(gpio_pin_get_dt(&sw3))
{
    deleteBond();
    printk("deleted bond");
}

if(gpio_pin_get_dt(&sw2))
{
    q_est.q1 = 1;
    q_est.q2 = 0;
    q_est.q3 = 0;
    q_est.q4 = 0;


    printk("reset orientation");
}
  
       
		// k_sleep(K_SECONDS(DELTA_T));
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
	poll_sensor(dev);
	return 0;
}