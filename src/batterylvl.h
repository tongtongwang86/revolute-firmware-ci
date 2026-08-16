// batterylvl.h
#ifndef BATTERYLVL_H
#define BATTERYLVL_H

#include <zephyr/kernel.h>
#include <stdbool.h>

void batteryThreadinit(void);
void sendbattery(void);
bool is_battery_empty(void);

/* Read state of charge once. Returns a percentage, or a negative errno if the
 * gauge could not be read. Callers in the sensor loop use this instead of
 * is_battery_empty() + sendbattery(), which would fetch twice per interval. */
int battery_read_percent(void);

/* Publish an already-read level over the Battery Service. */
void battery_publish(int percent);

/* True if the given level means the cell is flat enough to shut down. */
bool battery_level_is_empty(int percent);
void battery_stop(void);

/* Put the fuel gauge into its shutdown mode. It is powered directly from the
 * cell, so it keeps drawing its ~100uA operating current through System OFF
 * unless it is explicitly shut down. */
void battery_shutdown(void);

#endif // BATTERYLVL_H
