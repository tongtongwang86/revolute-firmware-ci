#ifndef MAGNETIC_H
#define MAGNETIC_H

#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>

#define THREAD_STACK_SIZE 1024
#define THREAD_PRIORITY 2



static double last_degree;
static double last_time;
static double last_speed;

static int direction;

static double speed_threshold;

static double continuous_counter ; 
static double dead_zone;     
static int numIdents;     
static int deviceMode;

extern uint16_t angle;

extern int change;

extern uint16_t CW_IDENT_OFFSET ; // angle offset in degrees
extern uint16_t CCW_IDENT_OFFSET ; // angle offset in degrees

void SensorThreadinit(void);
void suspend_magnetic_thread(void);
void resume_magnetic_thread(void);

bool is_battery_empty(void);
void sendbattery(void);

#endif //MAGNETIC