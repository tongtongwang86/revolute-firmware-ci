#ifndef POWER_H
#define POWER_H

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/policy.h>
#include <zephyr/pm/state.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/drivers/gpio.h>

/* Run states, in increasing order of activity.
 *
 * PWR_ON      - sensor rail up, polling at the fast rate.
 * PWR_HOLD    - the wheel has not moved for a while; sensor rail cycled at a
 *               slow rate so a nudge still wakes the device.
 * PWR_STANDBY - no magnet present, or the link is down; deepest state that
 *               still keeps Bluetooth alive.
 * PWR_OFF     - System OFF, wake on button only.
 */
enum power_type {
    PWR_OFF,
    PWR_STANDBY,
    PWR_HOLD,
    PWR_ON,
};

extern enum power_type power_status;

/* Full shutdown: parks every peripheral that can leak, then enters System OFF. */
void power_off(void);

/* Drop/restore the switched sensor rail and the I2C bus around it. */
void power_standby(void);
void power_resume(void);

/* Serialises use of i2c0 against power_standby()/power_resume(), which suspend
 * and resume the bus underneath any thread that happens to be using it. Threads
 * other than the sensor thread must hold this while talking to the bus. */
int i2c_bus_lock(k_timeout_t timeout);
void i2c_bus_unlock(void);

/* Inactivity shutdown. Reschedule from anywhere that counts as user activity. */
void power_off_timer_reschedule(void);
void schedule_power_off(int delay_ms);

#endif // POWER_H
