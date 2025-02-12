#ifndef MAGNETIC_H
#define MAGNETIC_H

#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>

#define THREAD_STACK_SIZE 1024
#define THREAD_PRIORITY 2


#define IDENT_OFFSET 8 // angle offset in degrees

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

extern uint8_t magnet_strength;

void SensorThreadinit(void);
void suspend_magnetic_thread(void);
void resume_magnetic_thread(void);


#endif //MAGNETIC