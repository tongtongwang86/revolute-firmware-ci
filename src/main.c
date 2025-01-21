#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/policy.h>
#include <zephyr/pm/state.h>
#include <zephyr/pm/device.h>
#include "hog.h"
#include "revsvc.h"
#include "batterylvl.h"
#include "button.h"
#include "ble.h"
#include "settings.h"
#include "led.h"
#include "magnetic.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* Prevent deep sleep (system off) from being entered on long timeouts
 * or `K_FOREVER` due to the default residency policy.
 *
 * This has to be done before anything tries to sleep, which means
 * before the threading system starts up between PRE_KERNEL_2 and
 * POST_KERNEL.  Do it at the start of PRE_KERNEL_2.
 */
// static int disable_ds_1(const struct device *dev)
// {
//     ARG_UNUSED(dev);

//     pm_policy_state_lock_get(PM_STATE_SOFT_OFF, PM_ALL_SUBSTATES);
//     return 0;
// }

// SYS_INIT(disable_ds_1, PRE_KERNEL_2, 0);

int main(void)
{
    // Initialize LED thread and GPIO
    // k_sleep(K_MSEC(2000));   // Sleep for 500 ms

    bluetooth_init();

    LOG_INF("rev_svc_loop thread started\n");

    batteryThreadinit();
    rev_svc_thread_init();
    button_init();



//  int err = led_init();
//     if (err < 0) {
//         LOG_ERR("LED initialization failed (err %d)", err);
//         return err;
//     }

    SensorThreadinit();


	int err = pwmled_init();
    if (err < 0) {
        LOG_ERR("LED initialization failed (err %d)", err);
        return err;
    }

    return 0;
}
