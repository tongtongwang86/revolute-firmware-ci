#ifndef SENSOR_THREAD_H
#define SENSOR_THREAD_H

#include <zephyr.h>

#define THREAD_STACK_SIZE 1024
#define THREAD_PRIORITY 5

/* Function declaration */
void sensor_thread(void);

extern struct k_thread sensor_thread_id;

extern uint16_t angle;

#endif /* SENSOR_THREAD_H */
