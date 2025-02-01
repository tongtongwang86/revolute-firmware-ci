#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/policy.h>
#include <zephyr/pm/state.h>
#include <zephyr/pm/device.h>
#include <zephyr/settings/settings.h>
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

    k_sleep(K_SECONDS(2));
    
    LOG_INF("Revooolluuuteee!\n");

    int err = zmk_ble_init();
    if (err < 0) {
        LOG_ERR("bluetooth init failed (err %d)", err);
        return err;
    }
    LOG_INF("bluetooth inited\n");

  
    

    // batteryThreadinit();
    rev_svc_thread_init();
    button_init();
    // LOG_INF("rev_svc_loop thread started\n");



    // SensorThreadinit();


	 err = pwmled_init();
    if (err < 0) {
        LOG_ERR("LED initialization failed (err %d)", err);
        return err;
    }

    return 0;
}
