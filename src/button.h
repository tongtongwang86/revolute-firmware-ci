#ifndef BUTTON_H
#define BUTTON_H

#include <zephyr/kernel.h>
#include <stdio.h>
#include <zephyr/sys/printk.h>
// #include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/settings/settings.h>
#include "power.h"

// extern led_state_t target_state;

// Thread configurations
#define REV_BUTTON_THREAD_STACK_SIZE 1024  // Adjust based on requirements
#define REV_BUTTON_THREAD_PRIORITY 5  // Priority for rev_button_loop thread

// Thread data and stack definitions
static struct k_thread rev_button_thread_data;
static K_THREAD_STACK_DEFINE(rev_button_stack, REV_BUTTON_THREAD_STACK_SIZE);


// Function declarations
void button_thread_fn(void *arg1, void *arg2, void *arg3);
void hog_button_thread(void *arg1, void *arg2, void *arg3);
void rev_button_thread_init(void);
void button_uninit(void);

#endif // BUTTON_H