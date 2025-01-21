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


int main(void)
{


    bluetooth_init();

    LOG_INF("rev_svc_loop thread started\n");

    batteryThreadinit();
    rev_svc_thread_init();
    button_init();



    SensorThreadinit();


	int err = pwmled_init();
    if (err < 0) {
        LOG_ERR("LED initialization failed (err %d)", err);
        return err;
    }

    return 0;
}
