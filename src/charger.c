#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

// 启用日志
LOG_MODULE_REGISTER(my_log_module, LOG_LEVEL_INF);

/* BQ25180 I2C Address */
#define BQ25180_I2C_ADDR 0x6A

/* Register Offsets */
#define STAT0_REG 0x00
#define STAT1_REG 0x01
#define FLAG0_REG 0x02
#define VBAT_CTRL_REG 0x03
#define ICHG_CTRL_REG 0x04
#define CHARGERCTRL0_REG 0x05
#define CHARGERCTRL1_REG 0x06
#define IC_CTRL_REG 0x07
#define TMR_ILIM_REG 0x08
#define SHIP_RST_REG 0x09
#define SYS_REG_REG 0x0A
#define TS_CONTROL_REG 0x0B
#define MASK_ID_REG 0x0C

/* I2C Device */
const struct device *i2c_dev;

/* Function to Write to a Register */
int bq25180_write_reg(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    return i2c_write(i2c_dev, buf, sizeof(buf), BQ25180_I2C_ADDR);
}

/* Function to Read from a Register */
int bq25180_read_reg(uint8_t reg, uint8_t *value) {
    return i2c_write_read(i2c_dev, BQ25180_I2C_ADDR, &reg, 1, value, 1);
}

/* Initialize the BQ25180 Charger */
void bq25180_init(void) {
    /* Set Battery Regulation Voltage to 4.2V */
    bq25180_write_reg(VBAT_CTRL_REG, 0x46); // 4.2V = 3.5V + (0x46 * 10mV)

    /* Set Fast Charge Current to 100mA */
    bq25180_write_reg(ICHG_CTRL_REG, 0x25); // 100mA = 40 + ((0x25 - 31) * 10)mA

    /* Configure Charger Control 0 */
    bq25180_write_reg(CHARGERCTRL0_REG, 0x90); // Precharge = 2x Term, Term = 10% ICHG, VINDPM = 4.5V, Thermal Reg = 80°C

    /* Configure Charger Control 1 */
    bq25180_write_reg(CHARGERCTRL1_REG, 0x16); // BATOCP = 200mA, BUVLO = 3.0V, Enable Charging Status Interrupt

    /* Configure IC Control */
    bq25180_write_reg(IC_CTRL_REG, 0x84); // Enable TS, VLOWV = 3.0V, VRCH = 100mV, 2x Timer, Safety Timer = 6h, Watchdog = 160s

    /* Configure Timer and Input Current Limit */
    bq25180_write_reg(TMR_ILIM_REG, 0x0D); // Long Press = 10s, Input Current Limit = 200mA

    /* Configure System Regulation */
    bq25180_write_reg(SYS_REG_REG, 0x40); // SYS Regulation = 4.5V, SYS Mode = Normal

    LOG_INF("BQ25180 Initialized");
}

/* Monitor Charger Status */
void bq25180_monitor(void) {
    uint8_t stat0, stat1, flag0;

    /* Read Status Registers */
    bq25180_read_reg(STAT0_REG, &stat0);
    bq25180_read_reg(STAT1_REG, &stat1);
    bq25180_read_reg(FLAG0_REG, &flag0);

    /* Print Status */
    LOG_INF("STAT0: 0x%02X, STAT1: 0x%02X, FLAG0: 0x%02X", stat0, stat1, flag0);

    /* Check Charging Status */
    if ((stat0 & 0x60) == 0x20) {
        LOG_INF("Charging: Constant Voltage Mode");
    } else if ((stat0 & 0x60) == 0x40) {
        LOG_INF("Charging: Constant Current Mode");
    } else if ((stat0 & 0x60) == 0x60) {
        LOG_INF("Charging Complete");
    }

    /* Check Faults */
    if (flag0 & 0x80) {
        LOG_INF("Fault: TS Fault Detected");
    }
    if (flag0 & 0x40) {
        LOG_INF("Fault: Input Current Limit Active");
    }
    if (flag0 & 0x20) {
        LOG_INF("Fault: DPPM Active");
    }
    if (flag0 & 0x10) {
        LOG_INF("Fault: VINDPM Active");
    }
    if (flag0 & 0x08) {
        LOG_INF("Fault: Thermal Regulation Active");
    }
    if (flag0 & 0x04) {
        LOG_INF("Fault: Input Overvoltage Detected");
    }
    if (flag0 & 0x02) {
        LOG_INF("Fault: Battery Undervoltage Detected");
    }
    if (flag0 & 0x01) {
        LOG_INF("Fault: Battery Overcurrent Detected");
    }
}

/* Main Application */
void charger(void) {
    /* Initialize I2C */
    i2c_dev = device_get_binding("I2C_0");
    if (!i2c_dev) {
        LOG_INF("I2C Device Not Found");
        return;
    }

    /* Initialize BQ25180 */
    bq25180_init();

    /* Monitor Charger Status */
    while (1) {
        bq25180_monitor();
        k_sleep(K_SECONDS(5)); // Check status every 5 seconds
    }
}    