#ifndef SENSOR_POLL_H
#define SENSOR_POLL_H

#include <zephyr/drivers/sensor.h>




// External sine and cosine lookup tables
// extern const int8_t sineLookupTable[];
// extern const int8_t cosineLookupTable[];

#define IDENT_OFFSET 1 // angle offset in degrees
#define STACKSIZE 1024
#define PRIORITY 2


// External variables
extern double last_degree;
extern double last_time;
extern double last_speed;

extern int direction;

extern double speed_threshold;

extern double continuous_counter ; 
extern double dead_zone;     
extern int numIdents;     
extern int deviceMode;

// Function declarations
double as5600_subdegree_refresh(const struct device *dev);
int as5600_refresh(const struct device *dev);
int predictive_update(double new_degree);
void print_direction(void);



#endif // SENSOR_POLL_H
