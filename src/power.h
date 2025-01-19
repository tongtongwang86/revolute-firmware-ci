#ifndef POWER_H
#define POWER_H

#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>

// Power states
typedef enum {
    STATE_OFF,
    STATE_PAIRING,
    STATE_ADVERTISEMENT,
    STATE_CONNECTED
} led_state_t;


extern led_state_t target_state;


#endif // POWER_H


