/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>


#define SW0_NODE	DT_ALIAS(sw0) 
#define LED0_NODE DT_ALIAS(led0)

LOG_MODULE_REGISTER(Rev,LOG_LEVEL_DBG);

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
             "Console device is not ACM CDC UART device");



int main(void) {


  if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

int ret;

ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
if (ret < 0) {
    return;
}

  const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
  uint32_t dtr = 0;

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
  if (enable_usb_device_next()) {
    return 0;
  }
#else
  if (usb_enable(NULL)) {
    return 0;
  }
#endif

  /* Poll if the DTR flag was set */
  while (!dtr) {
    uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
    /* Give CPU resources to low priority threads. */
    k_sleep(K_MSEC(100));
  }

  while (1) {
    printk("Hello World! %s\n", CONFIG_ARCH);
    LOG_INF("Exercise %d",2);   
    LOG_DBG("A log message in debug level");
    LOG_WRN("A log message in warning level!");
    LOG_ERR("A log message in Error level!");
    ret = gpio_pin_toggle_dt(&led);
     if (ret < 0) {
        return;
    }
    k_sleep(K_SECONDS(1));
  }
}
