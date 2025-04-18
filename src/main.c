/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 #include <zephyr/kernel.h>
 #include <zephyr/drivers/gpio.h>
 #include <zephyr/sys/printk.h>
 
 /* 1000 msec = 1 sec */
 #define SLEEP_TIME_MS   100
 
 /* 5000 msec = 5 seconds */
 #define LED_ON_TIME_MS  5000
 
 /* GPIO toggle interval */
 #define GPIO_TOGGLE_INTERVAL_MS 1000
 
 /* The devicetree node identifiers for the "led0" and "button2" aliases. */
 #define LED0_NODE DT_ALIAS(led0)
 #define BUTTON2_NODE DT_ALIAS(halleffect)
 
 /*
  * A build error on this line means your board is unsupported.
  * See the sample documentation for information on how to fix this.
  */
 static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
 static const struct gpio_dt_spec button2 = GPIO_DT_SPEC_GET(BUTTON2_NODE, gpios);
 
 /* GPIO pins for toggling */
 static const struct gpio_dt_spec gpio0_pin23 = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio0_pin23), gpios);
 static const struct gpio_dt_spec gpio1_pin15 = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio1_pin15), gpios);
 
 int main(void)
 {
     int ret;
     bool last_button_state = false;  // To store the previous button state
     bool led_on = false;  // To keep track of the LED state (on or off)
     int64_t last_gpio_toggle_time = 0;
     bool gpio_toggle_state = false;
 
     /* Check if the LED and button2 are ready */
     if (!gpio_is_ready_dt(&led) || !gpio_is_ready_dt(&button2)) {
         return 0;
     }
 
     /* Configure LED as output */
     ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
     if (ret < 0) {
         return 0;
     }
 
     /* Configure button2 as input with pull-up resistor */
     ret = gpio_pin_configure_dt(&button2, GPIO_INPUT | GPIO_PULL_UP);
     if (ret < 0) {
         return 0;
     }
 
     /* Configure GPIO pins for toggling */
     ret = gpio_pin_configure_dt(&gpio0_pin23, GPIO_OUTPUT_INACTIVE);
     if (ret < 0) {
         printk("Failed to configure GPIO0 pin 23\n");
         return 0;
     }
 
     ret = gpio_pin_configure_dt(&gpio1_pin15, GPIO_OUTPUT_INACTIVE);
     if (ret < 0) {
         printk("Failed to configure GPIO1 pin 15\n");
         return 0;
     }
 
     while (1) {
         int64_t current_time = k_uptime_get();
 
         /* Handle GPIO toggling */
         if (current_time - last_gpio_toggle_time >= GPIO_TOGGLE_INTERVAL_MS) {
             gpio_toggle_state = !gpio_toggle_state;
             
             /* Toggle GPIO0 pin 23 and GPIO1 pin 15 */
             gpio_pin_set_dt(&gpio0_pin23, gpio_toggle_state);
             gpio_pin_set_dt(&gpio1_pin15, !gpio_toggle_state);
             
             last_gpio_toggle_time = current_time;
         }
 
         /* Get the current button state */
         bool button2_pressed = !gpio_pin_get_dt(&button2);  // Active low logic
 
         /* Check if the button state has changed */
         if (button2_pressed != last_button_state) {
             /* Toggle the LED state and turn it on for 5 seconds */
             if (button2_pressed) {
                 // Button state changed to pressed, turn on LED
                 gpio_pin_set_dt(&led, 1);
                 printk("Button pressed, LED is ON\n");
                 led_on = true;
 
                 /* Wait for 5 seconds while LED stays on */
                 k_msleep(LED_ON_TIME_MS);
 
                 // After 5 seconds, turn off the LED
                 gpio_pin_set_dt(&led, 0);
                 printk("LED is OFF\n");
                 led_on = false;
             } else {
                 // Button state changed to released, we just toggle off the LED for now
                 printk("Button released, LED is OFF\n");
                 led_on = false;
             }
 
             /* Update last button state */
             last_button_state = button2_pressed;
         }
 
         /* Wait briefly to debounce the button */
         k_msleep(SLEEP_TIME_MS);
     }
 
     return 0;
 }