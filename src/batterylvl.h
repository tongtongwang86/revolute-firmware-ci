#ifndef BATTERYLEVEL_H
#define BATTERYLEVEL_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

#define STACKSIZE 1024
#define PRIORITY 7

void batteryThreadinit(void);

void suspend_battery_update(void);

void resume_battery_update(void);


#endif // BATTERYLEVEL_H