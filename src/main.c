#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/policy.h>
#include <zephyr/pm/state.h>
#include <zephyr/pm/device.h>


bool sysOn = true;

#define SW0_NODE DT_ALIAS(sw0)
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);


#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);



LOG_MODULE_REGISTER(Rev,LOG_LEVEL_DBG);

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{

	if(sysOn == true){
		LOG_INF("turned system OFF");
		gpio_pin_set_dt(&led,0);
		pm_state_force(0u, &(struct pm_state_info){PM_STATE_SOFT_OFF, 0, 0});


		sysOn = false;

	} else {
		LOG_INF("turned system ON");
		 gpio_pin_set_dt(&led,1);

		sysOn = true;
	}

}

static int disable_ds_1(const struct device *dev)
{
    ARG_UNUSED(dev);

    pm_policy_state_lock_get(PM_STATE_SOFT_OFF, PM_ALL_SUBSTATES);
    return 0;
}

SYS_INIT(disable_ds_1, PRE_KERNEL_2, 0);


static struct gpio_callback button_cb_data;


int main(void) {

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



  if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

  if (!device_is_ready(button.port)) {
	return -1;
}

int ret;

ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
if (ret < 0) {
    return;
}

ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret < 0) {
		return -1;
	}


  ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);

if (ret < 0) {
  LOG_ERR("set pin as interrupt failed");
	return -1;
}




    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin)); 	
	gpio_add_callback(button.port, &button_cb_data);


  while (1) {

    LOG_DBG("Hello World! %s\n", CONFIG_ARCH);
    
    k_sleep(K_MSEC(1000));
  }
}