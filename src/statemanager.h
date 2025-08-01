

#ifndef STATEMANAGER_H
#define STATEMANAGER_H

#include <zephyr/kernel.h>


//data types:

// typedef enum {
//     STATE_OFF,
//     STATE_PAIRING,
//     STATE_ADVERTISEMENT,
//     STATE_CONNECTED,
//     STATE_STANDBY,
//     STATE_ONFULL
// } ble_state_t;

// Define the config data structure
typedef struct {
    uint8_t deadzone;
    uint8_t up_report[8];
    uint8_t up_identPerRev;
    uint8_t up_transport;
    uint8_t dn_report[8];
    uint8_t dn_identPerRev;
    uint8_t dn_transport;
} rev_config_t;


typedef struct {
    uint32_t autoofftimer;
    uint32_t autoFilterOffTimer;
} rev_timer_t;

// Define the stats data structure
typedef struct {
    uint32_t quat_data[4]; // Quaternion data (floats packed as uint32_t)
    uint16_t rotation_value;
} rev_stats_t;

typedef enum {
    STATE_ON_HOLD,
    STATE_ON,
    STATE_OFF,
    STATE_PAIRING,
    STATE_ADVERTISEMENT,

} state_t;

extern bool onhold;
extern bool isOff;
extern rev_stats_t stats; // real time rotational data stats
extern rev_config_t config; // current configuration
// extern ble_state_t target_state; // target state for the BLE service
extern rev_timer_t timer; // auto off timer
extern state_t rev_state;


#endif