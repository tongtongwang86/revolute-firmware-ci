// batterylvl.h
#ifndef BATTERYLVL_H
#define BATTERYLVL_H

#include <zephyr/kernel.h>
#include <stdbool.h>

void batteryThreadinit(void);
void sendbattery(void);
bool is_battery_empty(void);
void battery_stop(void);

#endif // BATTERYLVL_H
