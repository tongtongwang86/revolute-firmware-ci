#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/sys/printk.h>

/* UART1 device */
#define UART1_NODE DT_NODELABEL(uart1)
static const struct device *uart1_dev = DEVICE_DT_GET(UART1_NODE);

/* GPIO devices from aliases in DTS */
#define TXEN_NODE DT_ALIAS(txen)
#define LOADEN_NODE DT_ALIAS(loaden)

static const struct gpio_dt_spec txen_gpio = GPIO_DT_SPEC_GET(TXEN_NODE, gpios);
static const struct gpio_dt_spec loaden_gpio = GPIO_DT_SPEC_GET(LOADEN_NODE, gpios);

static struct k_thread uart1_thread_data;
K_THREAD_STACK_DEFINE(uart1_stack_area, 1024);

static void uart1_tx_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    if (!device_is_ready(uart1_dev)) {
        printk("UART1 not ready\n");
        return;
    }
    if (!device_is_ready(txen_gpio.port) || !device_is_ready(loaden_gpio.port)) {
        printk("GPIOs not ready\n");
        return;
    }

    while (1) {
        /* Enable transmitter */
        // gpio_pin_set_dt(&txen_gpio, 1);

        // /* Send "C1" */
        // uart_poll_out(uart1_dev, 'C');
        // uart_poll_out(uart1_dev, '1');
        // uart_poll_out(uart1_dev, '\n'); // Optional newline

        // /* Disable transmitter */
        // gpio_pin_set_dt(&txen_gpio, 0);

        /* Enable load switch for remaining time */
        gpio_pin_set_dt(&loaden_gpio, 1);
        k_sleep(K_SECONDS(3));

        /* Turn off load switch */
        gpio_pin_set_dt(&loaden_gpio, 0);
        k_sleep(K_SECONDS(3));
    }
}

static int uart1_tx_init(void)
{
    /* Configure GPIOs */
    gpio_pin_configure_dt(&txen_gpio, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&loaden_gpio, GPIO_OUTPUT_INACTIVE);

    /* Create thread */
    k_thread_create(&uart1_thread_data, uart1_stack_area,
                    K_THREAD_STACK_SIZEOF(uart1_stack_area),
                    uart1_tx_thread,
                    NULL, NULL, NULL,
                    7, 0, K_NO_WAIT);
    k_thread_name_set(&uart1_thread_data, "uart1_tx");
    return 0;
}

SYS_INIT(uart1_tx_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
