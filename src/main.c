#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "hog.h"
#include "revsvc.h"
#include "batterylvl.h"
#include "button.h"
#include "ble.h"
#include "settings.h"
#include "led.h"
#include "magnetic.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void)
{
    // Initialize LED thread and GPIO
    k_sleep(K_MSEC(2000));   // Sleep for 500 ms

    bluetooth_init();

    LOG_INF("rev_svc_loop thread started\n");

    batteryThreadinit();
    rev_svc_thread_init();
    button_init();
 int err = led_init();
    if (err < 0) {
        LOG_ERR("LED initialization failed (err %d)", err);
        return err;
    }

    SensorThreadinit();


    return 0;
}
