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



void power_off(void);
void power_on(void);

#endif // POWER_H


