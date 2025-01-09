#ifndef GPIO_H
#define GPIO_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

// GPIO device tree node identifiers
#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)
#define SW2_NODE DT_ALIAS(sw2)
#define SW3_NODE DT_ALIAS(sw3)
#define LED0_NODE DT_ALIAS(led0)

// External declarations for the led variable
extern const struct gpio_dt_spec led;

extern const struct gpio_dt_spec buttons[];

int ledInit(void);

// Button event definitions
enum button_evt {
    BUTTON_EVT_PRESSED,
    BUTTON_EVT_RELEASED
};

typedef void (*button_event_handler_t)(size_t idx, enum button_evt evt);

// Function declarations for button handling
int button_init(size_t idx, button_event_handler_t handler);

#endif // GPIO_H
