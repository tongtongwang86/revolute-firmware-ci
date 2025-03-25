#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define BQ25180_I2C_ADDR 0x6A

// Register addresses from datasheet
#define BQ25180_STAT0_REG 0x00
#define BQ25180_STAT1_REG 0x01

const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
const struct gpio_dt_spec bq25180_int = GPIO_DT_SPEC_GET(DT_NODELABEL(bq25180), int_gpios);

static K_SEM_DEFINE(i2c_sem, 1, 1);  // I2C access semaphore

static int read_bq25180_reg(uint8_t reg, uint8_t *val)
{
    int ret;
    
    if (k_is_in_isr()) {
        // If we're in interrupt context, try to take the semaphore without waiting
        if (k_sem_take(&i2c_sem, K_NO_WAIT) != 0) {
            return -EBUSY;
        }
    } else {
        k_sem_take(&i2c_sem, K_FOREVER);
    }
    
    ret = i2c_reg_read_byte(i2c_dev, BQ25180_I2C_ADDR, reg, val);
    k_sem_give(&i2c_sem);
    
    return ret;
}

static void update_charging_status(void)
{
    uint8_t stat0, stat1;
    static enum {
        STATE_UNKNOWN,
        STATE_NOT_CHARGING,
        STATE_CHARGING_CC,
        STATE_CHARGING_CV,
        STATE_FULL
    } last_state = STATE_UNKNOWN;
    
    if (read_bq25180_reg(BQ25180_STAT0_REG, &stat0) < 0) {
        LOG_ERR("Failed to read STAT0");
        return;
    }
    
    if (read_bq25180_reg(BQ25180_STAT1_REG, &stat1) < 0) {
        LOG_ERR("Failed to read STAT1");
        return;
    }
    
    // Decode status (from datasheet page 28)
    bool vin_present = stat0 & BIT(0);
    uint8_t chg_status = (stat0 >> 5) & 0x03;
    
    if (!vin_present) {
        if (last_state != STATE_NOT_CHARGING) {
            LOG_INF("Not charging (No input voltage)");
            last_state = STATE_NOT_CHARGING;
        }
        return;
    }
    
    switch (chg_status) {
        case 0b01:  // Constant Current
            if (last_state != STATE_CHARGING_CC) {
                LOG_INF("Charging (Constant Current)");
                last_state = STATE_CHARGING_CC;
            }
            break;
            
        case 0b10:  // Constant Voltage
            if (last_state != STATE_CHARGING_CV) {
                LOG_INF("Charging (Constant Voltage)");
                last_state = STATE_CHARGING_CV;
            }
            break;
            
        case 0b11:  // Charge Complete
            if (last_state != STATE_FULL) {
                LOG_INF("Battery fully charged");
                last_state = STATE_FULL;
            }
            break;
            
        default:  // Not charging
            if (last_state != STATE_NOT_CHARGING) {
                LOG_INF("Not charging (Input present)");
                last_state = STATE_NOT_CHARGING;
            }
            break;
    }
}

static void bq25180_int_callback(const struct device *dev, struct gpio_callback *cb,
                                uint32_t pins)
{
    // Schedule work to handle the interrupt in thread context
    update_charging_status();
}

void main(void) 
{
    static struct gpio_callback int_cb_data;
    
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C device not ready");
        return;
    }

    // Configure interrupt pin
    if (!device_is_ready(bq25180_int.port)) {
        LOG_ERR("Interrupt GPIO controller not ready");
        return;
    }

    int ret = gpio_pin_configure_dt(&bq25180_int, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("Could not configure interrupt GPIO");
        return;
    }

    ret = gpio_pin_interrupt_configure_dt(&bq25180_int, 
                                        GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        LOG_ERR("Could not configure interrupt");
        return;
    }

    gpio_init_callback(&int_cb_data, bq25180_int_callback, BIT(bq25180_int.pin));
    gpio_add_callback(bq25180_int.port, &int_cb_data);

    // Initial status check
    update_charging_status();

    while (1) {
        k_sleep(K_SECONDS(10));  // Periodic check in case we miss interrupts
        update_charging_status();
    }
}