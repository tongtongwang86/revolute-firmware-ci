#ifndef BLE_H
#define BLE_H

#include <zephyr/bluetooth/addr.h>

#define ZMK_BLE_PROFILE_NAME_MAX 15

struct zmk_ble_profile {
    char name[ZMK_BLE_PROFILE_NAME_MAX];
    bt_addr_le_t peer;
};

struct zmk_ble_active_profile_changed {
    uint8_t index;
    struct zmk_ble_profile *profile;
};

#define ZMK_BLE_PROFILE_COUNT CONFIG_BT_MAX_PAIRED

void zmk_ble_clear_bonds(void);
int zmk_ble_prof_next(void);
int zmk_ble_prof_prev(void);
int zmk_ble_prof_select(uint8_t index);
void zmk_ble_clear_all_bonds(void);
int zmk_ble_prof_disconnect(uint8_t index);

int zmk_ble_active_profile_index(void);
int zmk_ble_profile_index(const bt_addr_le_t *addr);

bt_addr_le_t *zmk_ble_active_profile_addr(void);
struct bt_conn *zmk_ble_active_profile_conn(void);

bool zmk_ble_active_profile_is_open(void);
bool zmk_ble_active_profile_is_connected(void);
char *zmk_ble_active_profile_name(void);

int zmk_ble_unpair_all(void);

int zmk_ble_set_device_name(char *name);

int zmk_ble_init(void);

#endif // BLE_H
