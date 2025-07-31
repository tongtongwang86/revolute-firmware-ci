#include "statemanager.h"

ble_state_t target_state = STATE_ADVERTISEMENT;

// initial configuration

rev_config_t config = {
    .deadzone = 0x00,
    .up_report = {0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00},
    .up_identPerRev = 0x1E,
    .up_transport = 0x0D, // 5 keyboard, 9 consumer, 13 mouse
    .dn_report = {0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00},
    .dn_identPerRev = 0x1E,
    .dn_transport = 0x0D // 5 keyboard, 9 consumer, 13 mouse
};


rev_timer_t timer = {
    .autoofftimer = 0,
    .autoFilterOffTimer = 300000
};

rev_stats_t stats = {
    .quat_data = {0x3f800000, 0x00000000, 0x00000000, 0x00000000}, // Quaternion identity (1.0, 0.0, 0.0, 0.0)
    .rotation_value = 0x0001 // Example rotation value
};


