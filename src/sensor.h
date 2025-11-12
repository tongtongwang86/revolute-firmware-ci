// sensor.h
#ifndef SENSOR_H
#define SENSOR_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <stdint.h>

typedef void (*rotation_callback_t)(void);

void sensor_init(void);
void sensor_read(void);
void configure_tlv493_poweroff_mode(void);
void tlv493_general_reset(void);
void sensor_stop(void);

void register_cw_callback(rotation_callback_t cb);
void register_ccw_callback(rotation_callback_t cb);

float sensor_get_strength(int16_t bx, int16_t by);
void sensor_set_degree_threshold(float threshold);
void set_cw_identsperrev();
void set_ccw_identsperrev();

#endif // SENSOR_H
