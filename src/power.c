
#include <zephyr/kernel.h>
#include "power.h"
#include "sensor.h"
#include "batterylvl.h"
#include "charger.h"
#include "pwmled.h"
#include "button.h"
#include "ble.h"
#include "statemanager.h"
#include <zephyr/sys/poweroff.h>

LOG_MODULE_REGISTER(power, LOG_LEVEL_INF);

#define SW3_NODE DT_ALIAS(sw0) // button to attach the interrupt to
static const struct gpio_dt_spec sw3_button = GPIO_DT_SPEC_GET(SW3_NODE, gpios);

/* Switched rail feeding the magnetic sensor. Q5 is a P-channel high-side switch,
 * so a low gate turns the rail ON; the alias is declared active-high, which makes
 * gpio_pin_set_dt(&mosfet, 1) the "rail off" call. Not present on the devkit
 * overlay, so it is optional. */
#if DT_NODE_EXISTS(DT_ALIAS(mosfet))
#define HAVE_SENSOR_RAIL 1
static const struct gpio_dt_spec mosfet = GPIO_DT_SPEC_GET(DT_ALIAS(mosfet), gpios);
#endif

#define I2C_NODE DT_NODELABEL(i2c0)

enum power_type power_status = PWR_ON;

/* Default inactivity shutdown when the configuration does not specify one. */
#define DEFAULT_AUTO_OFF_MS (10 * 60 * 1000)

static struct k_work_delayable power_off_work;

K_MUTEX_DEFINE(i2c_bus_mutex);

int i2c_bus_lock(k_timeout_t timeout)
{
    return k_mutex_lock(&i2c_bus_mutex, timeout);
}

void i2c_bus_unlock(void)
{
    k_mutex_unlock(&i2c_bus_mutex);
}

static void power_off_handler(struct k_work *work)
{
    LOG_INF("Auto-off timer expired, powering off");
    power_off();
}

static int power_work_init(void)
{
    k_work_init_delayable(&power_off_work, power_off_handler);
    power_off_timer_reschedule();
    return 0;
}

void schedule_power_off(int delay_ms)
{
    LOG_INF("Scheduling power off in %d ms", delay_ms);
    k_work_reschedule(&power_off_work, K_MSEC(delay_ms));
}

void power_off_timer_reschedule(void)
{
    /* Which timer applies depends on whether we are in use or just looking for
     * a host: a device sitting in advertising should give up sooner than one
     * that is connected. A configured value of 0 disables that timer. */
    uint32_t delay_ms = (advertising_status == ADV_NONE) ? timer.autoofftimer
                                                         : timer.autoFilterOffTimer;

    if (delay_ms == 0) {
        delay_ms = DEFAULT_AUTO_OFF_MS;
    }

    k_work_reschedule(&power_off_work, K_MSEC(delay_ms));
}

static void i2c_suspend(void)
{
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

    if (!device_is_ready(i2c_dev)) {
        return;
    }

    /* Suspending applies the i2c0 "sleep" pinctrl state, which disconnects
     * P0.15/P0.17. Without this the bus pins stay driven into a sensor whose
     * supply has just been cut, and current flows through its ESD clamps into
     * the dead rail. */
    int ret = pm_device_action_run(i2c_dev, PM_DEVICE_ACTION_SUSPEND);

    if (ret == 0 || ret == -EALREADY) {
        LOG_INF("I2C suspended");
    } else {
        LOG_WRN("Failed to suspend I2C (err %d)", ret);
    }
}

static void i2c_resume(void)
{
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

    if (!device_is_ready(i2c_dev)) {
        LOG_WRN("I2C device is not ready");
        return;
    }

    int ret = pm_device_action_run(i2c_dev, PM_DEVICE_ACTION_RESUME);

    if (ret && ret != -EALREADY) {
        LOG_WRN("Failed to resume I2C (err %d)", ret);
    }
}

static void sensor_rail_set(bool on)
{
#ifdef HAVE_SENSOR_RAIL
    if (!device_is_ready(mosfet.port)) {
        return;
    }

    /* Alias is active-high and the switch is a PMOS: 1 = rail off. */
    gpio_pin_configure_dt(&mosfet, GPIO_OUTPUT);
    gpio_pin_set_dt(&mosfet, on ? 0 : 1);
#else
    ARG_UNUSED(on);
#endif
}

void power_standby(void)
{
    /* Wait for any other user of the bus to finish before pulling it out from
     * under them. */
    (void)k_mutex_lock(&i2c_bus_mutex, K_MSEC(100));
    i2c_suspend();
    sensor_rail_set(false);
    k_mutex_unlock(&i2c_bus_mutex);
    LOG_INF("standby");
}

void power_resume(void)
{
    (void)k_mutex_lock(&i2c_bus_mutex, K_MSEC(100));
    sensor_rail_set(true);
    i2c_resume();
    k_mutex_unlock(&i2c_bus_mutex);

    /* The sensor lost its configuration with its supply, and the rotation
     * tracker's reference vector is stale. */
    sensor_reinit_after_resume();
    LOG_INF("resumed");
}

/* The shutdown is triggered by a long press, so the button is still held. A
 * level-triggered wake source would fire the instant we enter System OFF, so
 * wait for the release before arming it. */
static void wait_for_button_release(void)
{
    const int64_t deadline = k_uptime_get() + 5000;

    while (gpio_pin_get_dt(&sw3_button) > 0 && k_uptime_get() < deadline) {
        k_msleep(20);
    }
}

void power_off(void)
{
    power_status = PWR_OFF;
    isOff = true;

    k_work_cancel_delayable(&power_off_work);

    LOG_INF("Preparing to power off");

    /* 1. Visible feedback, and park the LED. The nRF52 retains GPIO output
     *    state through System OFF, so whatever duty cycle is latched here is
     *    what the LED keeps for as long as the device is "off". */
    pwmled_shutdown();

    /* 2. Stop everything that talks to the I2C bus, while the bus still works. */
    sensor_stop();  /* also puts the TLV493D into power-down mode */
    battery_stop();
    charger_stop();

    /* 3. Put the fuel gauge into shutdown. In its normal operating mode the
     *    BQ27427 draws ~100uA continuously from the battery, which on its own
     *    is two orders of magnitude over the System OFF budget. */
    battery_shutdown();

    /* 4. Radio and button interrupts. */
    disable_bluetooth();
    button_uninit();

    /* 5. Cut the sensor rail, then disconnect the bus pins feeding it. Order
     *    matters: the bus is needed for steps 2-3. */
    (void)k_mutex_lock(&i2c_bus_mutex, K_MSEC(100));
    sensor_rail_set(false);
    i2c_suspend();
    k_mutex_unlock(&i2c_bus_mutex);

    wait_for_button_release();

    /* 6. Arm the wake source. */
    int rc = gpio_pin_configure_dt(&sw3_button, GPIO_INPUT);

    if (rc < 0) {
        LOG_ERR("Failed to configure SW0 GPIO (%d)", rc);
        return;
    }

    rc = gpio_pin_interrupt_configure_dt(&sw3_button, GPIO_INT_LEVEL_ACTIVE);
    if (rc < 0) {
        LOG_ERR("Failed to configure interrupt for SW0 GPIO (%d)", rc);
        return;
    }

    LOG_INF("shutting off");
    sys_poweroff();
}

SYS_INIT(power_work_init, APPLICATION, 60);
