#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>
LOG_MODULE_REGISTER(Revolute, LOG_LEVEL_DBG);
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

#define I2C_NODE DT_NODELABEL(i2c0) // Referencing the i2c0 node

void main(void) {
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

    if (!device_is_ready(i2c_dev)) {
        printk("I2C device not found\n");
        return;
    }

    printk("I2C device found: %s\n", i2c_dev->name);

    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        struct i2c_msg msgs[1];
        uint8_t dummy = 0;

        msgs[0].buf = &dummy;
        msgs[0].len = 1U;
        msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

        int ret = i2c_transfer(i2c_dev, msgs, 1, addr);

        if (ret == 0) {
            printk("I2C device found at address: 0x%02X\n", addr);
        }
    }

    printk("I2C scan complete.\n");
}
