#include "led.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(led, LOG_LEVEL_DBG);

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

// Work queue and thread definitions
#define LED_STACK_SIZE 1024
#define LED_THREAD_PRIORITY K_LOWEST_APPLICATION_THREAD_PRIO
#define WORK_QUEUE_PRIORITY K_LOWEST_APPLICATION_THREAD_PRIO

static struct k_thread led_thread_data;
static K_THREAD_STACK_DEFINE(led_stack, LED_STACK_SIZE);
static K_THREAD_STACK_DEFINE(led_work_q_stack, LED_STACK_SIZE);
static struct k_work_q led_work_q;

// Work items
static struct k_work led_toggle_work;
static struct k_work led_notify_work;

// LED initialization
static int led_configure(void)
{
    if (!device_is_ready(led.port)) {
        LOG_ERR("LED device %s not ready", led.port->name);
        return -ENODEV;
    }

    int err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (err < 0) {
        LOG_ERR("Failed to configure LED GPIO (err %d)", err);
        return err;
    }

    LOG_INF("LED GPIO configured successfully");
    return 0;
}

// Work handler for toggling the LED
static void led_toggle_handler(struct k_work *work)
{
    gpio_pin_toggle_dt(&led);
    LOG_DBG("LED toggled");
}

// Work handler for notifying via LED
static void led_notify_handler(struct k_work *work)
{
    for (int i = 0; i < 4; i++) {
        gpio_pin_toggle_dt(&led);
        k_sleep(K_MSEC(100));
    }
    LOG_DBG("LED notify sequence completed");
}

// LED thread entry point
static void led_thread(void *unused1, void *unused2, void *unused3)
{
    LOG_INF("LED thread started");

    // Periodically schedule LED toggle
    while (1) {
        k_sleep(K_MSEC(500));  // Sleep for 500 ms before toggling LED
        k_work_submit_to_queue(&led_work_q, &led_toggle_work);  // Submit toggle work to queue
        LOG_INF("LED toggle work submitted");
    }
}

// External API to trigger notify work
void led_notify_trigger(void)
{
    k_work_submit_to_queue(&led_work_q, &led_notify_work);
    LOG_INF("LED notify work triggered");
}

int led_init(void)
{
    int err = led_configure();
    if (err < 0) {
        return err;
    }

    // Start work queue with its dedicated stack
    k_work_queue_start(&led_work_q, led_work_q_stack, K_THREAD_STACK_SIZEOF(led_work_q_stack),
                       WORK_QUEUE_PRIORITY, NULL);

    // Ensure work queue is initialized before starting thread
    k_sleep(K_MSEC(1));

    // Start the LED thread
    k_thread_create(&led_thread_data, led_stack, K_THREAD_STACK_SIZEOF(led_stack),
                    led_thread, NULL, NULL, NULL,
                    LED_THREAD_PRIORITY, 0, K_NO_WAIT);

    // Initialize the work items
    k_work_init(&led_toggle_work, led_toggle_handler);
    k_work_init(&led_notify_work, led_notify_handler);

    return 0;
}
