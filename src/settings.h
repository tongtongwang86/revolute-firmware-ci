
#ifndef SETTINGS_H
#define SETTINGS_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include "revsvc.h"



//data types:

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


extern rev_config_t config;

extern rev_timer_t timer;

ssize_t read_callback(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
ssize_t write_callback(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags);

void save_config(void);
void load_config(void);



#endif