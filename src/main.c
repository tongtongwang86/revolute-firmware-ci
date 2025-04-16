#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define BQ25180_I2C_ADDR 0x6A
#define MAX_RETRIES 3
#define RETRY_DELAY_MS 10
#define POLL_INTERVAL_MS 1000

// Register addresses
#define BQ25180_STAT0_REG 0x00
#define BQ25180_STAT1_REG 0x01

// Status bits
#define VIN_PGOOD_STAT  BIT(0)
#define CHG_STAT_MASK   0b11000000

const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
const struct gpio_dt_spec bq25180_int = GPIO_DT_SPEC_GET(DT_NODELABEL(bq25180), int_gpios);
const struct gpio_dt_spec gpio0_pin2 = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio0_pin2), gpios);
const struct gpio_dt_spec gpio0_pin28 = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio0_pin28), gpios);

static struct k_work_delayable status_work;
static atomic_t vin_changed = ATOMIC_INIT(false);

static int read_bq25180_reg_with_retry(uint8_t reg, uint8_t *val)
{
    int ret;
    int attempts = 0;
    
    do {
        ret = i2c_reg_read_byte(i2c_dev, BQ25180_I2C_ADDR, reg, val);
        if (ret == 0) return 0;
        k_msleep(RETRY_DELAY_MS);
    } while (++attempts < MAX_RETRIES);
    
    return ret;
}

static void set_switches(bool load_switch_enabled)
{
    // Set load switch (gpio0_pin2) and analog switch (gpio0_pin28) with complementary logic
    gpio_pin_set_dt(&gpio0_pin2, load_switch_enabled ? 1 : 0);
    gpio_pin_set_dt(&gpio0_pin28, load_switch_enabled ? 0 : 1);
    
    LOG_INF("Load switch: %s, Analog switch: %s", 
            load_switch_enabled ? "ON" : "OFF",
            load_switch_enabled ? "OFF" : "ON");
}

static void report_status(struct k_work *work)
{
    uint8_t stat0;
    static bool last_vin = false;
    static uint8_t last_chg_state = 0xFF;
    
    if (read_bq25180_reg_with_retry(BQ25180_STAT0_REG, &stat0) < 0) {
        LOG_ERR("Status read failed");
        goto reschedule;
    }

    bool current_vin = stat0 & VIN_PGOOD_STAT;
    uint8_t current_chg_state = (stat0 & CHG_STAT_MASK) >> 6;

    // Handle VIN changes
    if (current_vin != last_vin || atomic_get(&vin_changed)) {
        LOG_INF("Input voltage: %s", current_vin ? "Present" : "Absent");
        last_vin = current_vin;
        atomic_set(&vin_changed, false);
        
        // Set switches based on VIN presence
        set_switches(current_vin);
    }

    // Handle charging state changes
    if (current_vin && current_chg_state != last_chg_state) {
        const char *states[] = {
            "Not Charging", 
            "Constant Current", 
            "Constant Voltage", 
            "Fully Charged"
        };
        LOG_INF("Charging state: %s", states[current_chg_state]);
        last_chg_state = current_chg_state;
    } else if (!current_vin && last_chg_state != 0xFF) {
        LOG_INF("Charging state: Not Charging (No input)");
        last_chg_state = 0xFF;
    }

reschedule:
    k_work_reschedule(&status_work, K_MSEC(last_vin ? 5000 : POLL_INTERVAL_MS));
}

static void bq25180_int_callback(const struct device *dev,
                                struct gpio_callback *cb,
                                uint32_t pins)
{
    atomic_set(&vin_changed, true);
    k_work_reschedule(&status_work, K_NO_WAIT);
}

void main(void)
{
    static struct gpio_callback int_cb_data;

    // Initialize GPIO0 pin2 (load switch) and pin28 (analog switch) as outputs
    if (!device_is_ready(gpio0_pin2.port) || !device_is_ready(gpio0_pin28.port)) {
        LOG_ERR("GPIO controllers not ready");
        return;
    }
    
    if (gpio_pin_configure_dt(&gpio0_pin2, GPIO_OUTPUT_INACTIVE) < 0 ||
        gpio_pin_configure_dt(&gpio0_pin28, GPIO_OUTPUT_INACTIVE) < 0) {
        LOG_ERR("Failed to configure GPIO pins");
        return;
    }
    
    // Initialize switches with load switch disabled and analog switch enabled
    set_switches(false);
    LOG_INF("GPIOs initialized - Load switch OFF, Analog switch ON");

    // Initialize I2C and interrupt
    if (!device_is_ready(i2c_dev) || !device_is_ready(bq25180_int.port)) {
        LOG_ERR("Hardware not ready");
        return;
    }

    int ret = gpio_pin_configure_dt(&bq25180_int, GPIO_INPUT);
    ret |= gpio_pin_interrupt_configure_dt(&bq25180_int, GPIO_INT_EDGE_BOTH);
    if (ret < 0) {
        LOG_ERR("Interrupt setup failed");
        return;
    }

    gpio_init_callback(&int_cb_data, bq25180_int_callback, BIT(bq25180_int.pin));
    gpio_add_callback(bq25180_int.port, &int_cb_data);

    // Initialize status monitoring
    k_work_init_delayable(&status_work, report_status);
    k_work_reschedule(&status_work, K_NO_WAIT);

    while (1) {
        k_sleep(K_SECONDS(30));
        atomic_set(&vin_changed, true);
        k_work_reschedule(&status_work, K_NO_WAIT);
    }
}