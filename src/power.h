#ifndef POWER_H
#define POWER_H

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/policy.h>
#include <zephyr/pm/state.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/drivers/gpio.h>

// Power states
typedef enum {
    STATE_OFF,
    STATE_PAIRING,
    STATE_ADVERTISEMENT,
    STATE_CONNECTED,
    STATE_STANDBY,
    STATE_ONFULL
} led_state_t;

extern led_state_t target_state;

void power_off(void);

#endif // POWER_H


