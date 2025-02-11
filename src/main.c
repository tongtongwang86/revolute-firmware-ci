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
    LOG_INF("Revooolluuuteee!\n");

    return 0;
}
