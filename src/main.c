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
 
 /* The devicetree node identifiers for the "led0" and "button2" aliases. */
 #define LED0_NODE DT_ALIAS(led0)
 #define BUTTON2_NODE DT_ALIAS(halleffect)
 
 /*
  * A build error on this line means your board is unsupported.
  * See the sample documentation for information on how to fix this.
  */
 static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
 static const struct gpio_dt_spec button2 = GPIO_DT_SPEC_GET(BUTTON2_NODE, gpios);
 
 int main(void)
 {
	 int ret;
	 bool last_button_state = false;  // To store the previous button state
	 bool led_on = false;  // To keep track of the LED state (on or off)
 
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
 
	 while (1) {
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
 