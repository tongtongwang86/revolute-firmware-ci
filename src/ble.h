#ifndef BLE_H
#define BLE_H

#include <zephyr/bluetooth/addr.h>

#define ZMK_BLE_PROFILE_NAME_MAX 15

enum advertising_type {
    ADV_NONE,
    ADV_FILTER,
    ADV_CONN,
};
extern enum advertising_type advertising_status;

#define ZMK_BLE_PROFILE_COUNT CONFIG_BT_MAX_PAIRED

bool zmk_ble_active_profile_is_connected(void);
bool is_active_profile_any(void);
int remove_bonded_device(void);

int zmk_ble_set_device_name(char *name);



#endif // BLE_H
