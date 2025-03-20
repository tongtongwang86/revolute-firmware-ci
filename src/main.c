#include <zephyr/device.h>
#include <zephyr/drivers/charger.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

//#include <zephyr/pm/pm.h>
//#include <zephyr/pm/policy.h>
//#include <zephyr/pm/state.h>
//#include <zephyr/pm/device.h>
//#include <zephyr/settings/settings.h>
//#include "hog.h"
//#include "revsvc.h"
//#include "batterylvl.h"
//#include "button.h"
//#include "ble.h"
//#include "settings.h"
//#include "led.h"
//#include "magnetic.h"

//LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

//int main(void)
//{
//    LOG_INF("Revooolluuuteee!\n");
//
//    return 0;
//}


const struct device *charger;

void main(void) {
    charger = DEVICE_DT_GET(DT_NODELABEL(bq25180));
    
    if (!device_is_ready(charger)) {
        LOG_ERR("BQ25180 charger is not ready!");
        return;
    }

    union charger_propval val;
    
    // Set charge current to 100mA
    val.const_charge_current_ua = 100000;  // 100mA in microamps
    if (charger_set_prop(charger, CHARGER_PROP_CONSTANT_CHARGE_CURRENT_UA, &val) < 0) {
        LOG_ERR("Failed to set charge current");
    } else {
        LOG_INF("Charge current set to 100mA");
    }
}
