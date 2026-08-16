#ifndef HOG_H
#define HOG_H

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

/* Function to initialize the HoG service */
void hog_init(void);

/* Queue one detent in each direction. Both the press and its release are queued
 * as a single unit, so a release can never be lost on its own. */
void revolute_up_submit(void);
void revolute_dn_submit(void);

/* Queue one continuous (relative) report carrying a signed magnitude. */
void revolute_up_cont_submit(int8_t delta);
void revolute_dn_cont_submit(int8_t delta);

/* Number of ticks discarded because the send queue overflowed. */
uint32_t revolute_dropped_tick_count(void);


/* Function to send a mouse button 1 press via GATT notification */
// void hog_send_mouse_button_1(void);

#endif /* HOG_H */