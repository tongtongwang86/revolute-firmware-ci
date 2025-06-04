#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/devicetree.h>

// Match the node from your devicetree
#define TLV493_NODE DT_NODELABEL(tlv493)

static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(TLV493_NODE);

// Function to sign-extend and assemble 12-bit values
static int16_t extract_12bit(uint8_t msb, uint8_t lsb_part) {
    int16_t val = ((int16_t)msb << 4) | (lsb_part & 0x0F);
    if (val & 0x800) {
        val |= 0xF000;  // sign extend to 16 bits
    }
    return val;
}


static uint8_t calculate_parity(uint8_t reg0, uint8_t reg1, uint8_t reg2, uint8_t reg3) {
    uint8_t bits[4] = {reg0, reg1, reg2, reg3};
    uint8_t bit_sum = 0;
    for (int i = 0; i < 4; i++) {
        for (int b = 0; b < 8; b++) {
            bit_sum += (bits[i] >> b) & 1;
        }
    }
    return (bit_sum % 2 == 0) ? 1 : 0; // Parity must be odd
}

static void configure_tlv493_low_power_mode_preserve(void) {
    uint8_t reg0 = 0, reg1 = 0, reg2 = 0, reg3 = 0;

    // Read registers 0x00 through 0x03
    if (i2c_reg_read_byte_dt(&dev_i2c, 0x00, &reg0) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x01, &reg1) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x02, &reg2) < 0 ||
        i2c_reg_read_byte_dt(&dev_i2c, 0x03, &reg3) < 0) {
        printk("Failed to read configuration registers\n");
        return;
    }

    // Clear bits 2:0 (INT, FAST, LOW)
    reg1 &= ~0x07;

    // Set INT = 0, FAST = 0, LOW = 1
    reg1 |= (1 << 0);  // LOW = 1

    // Calculate new parity over all 4 config bytes
    uint8_t parity = calculate_parity(reg0, reg1, reg2, reg3);

    // Set parity bit (bit 7)
    reg1 = (reg1 & 0x7F) | (parity << 7);

    // Write back only modified register (reg1 at address 0x01)
    int ret = i2c_reg_write_byte_dt(&dev_i2c, 0x01, reg1);
    if (ret < 0) {
        printk("Failed to write MOD1 register: %d\n", ret);
    } else {
        printk("Low-power mode configured, MOD1 updated with preserved bits.\n");
    }
}




static void i2c_scan_bus(const struct i2c_dt_spec *i2c) {
    printk("Starting I2C scan on bus %s...\n", i2c->bus->name);

    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        struct i2c_msg msgs[1];
        uint8_t dummy = 0;

        msgs[0].buf = &dummy;
        msgs[0].len = 1;
        msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

        int ret = i2c_transfer(i2c->bus, msgs, 1, addr);
        if (ret == 0) {
            printk("I2C device found at address 0x%02X\n", addr);
        }
    }

    printk("I2C scan complete.\n");
}




static void tlv493_general_reset(const struct i2c_dt_spec *i2c) {
    struct i2c_msg msg = {
        .buf = NULL,
        .len = 0,
        .flags = I2C_MSG_WRITE | I2C_MSG_STOP,
    };

    int ret = i2c_transfer(i2c->bus, &msg, 1, 0x00);  // Address 0x00 triggers general reset
    if (ret < 0) {
        printk("General reset (zero-length) failed: %d\n", ret);
    } else {
        k_busy_wait(100);  // t1 > 14µs
        printk("TLV493D general reset (zero-length) done.\n");
    }
}

static void read_magnetic_data(void) {
    uint8_t raw[6];
    int ret = i2c_burst_read_dt(&dev_i2c, 0x00, raw, sizeof(raw));
    
    if (ret < 0) {
        printk("Failed to read sensor data: %d. Attempting reinitialization...\n", ret);

        // Step 1: General reset
        tlv493_general_reset(&dev_i2c);
        k_msleep(5);  // Allow time for sensor reset

        // Step 2: Reconfigure low-power mode
        configure_tlv493_low_power_mode_preserve();

        // Step 3: Retry reading data
        ret = i2c_burst_read_dt(&dev_i2c, 0x00, raw, sizeof(raw));
        if (ret < 0) {
            printk("Retry failed: Sensor is still unresponsive.\n");
            return;
        } else {
            printk("Reinitialization succeeded.\n");
        }
    }

    int16_t bx = extract_12bit(raw[0], raw[4] >> 4);
    int16_t by = extract_12bit(raw[1], raw[4]);
    int16_t bz = extract_12bit(raw[2], raw[5]);

    printk("%d, %d, %d,\n", bx, by, bz);
}


void main(void) {
    if (!device_is_ready(dev_i2c.bus)) {
        printk("I2C bus %s is not ready!\n", dev_i2c.bus->name);
        return;
    }
	    i2c_scan_bus(&dev_i2c);                 // 🔍 Scan for devices first
	    tlv493_general_reset(&dev_i2c);         // 🛠️ Reset the sensor
		k_msleep(5); // Wait 5 milliseconds
    i2c_scan_bus(&dev_i2c);       
	read_magnetic_data();         // 🔍 Scan for devices first
	// configure_tlv493_low_power_mode_full();
    configure_tlv493_low_power_mode_preserve(); // 💡 Must be called first

    while (1) {
        read_magnetic_data();
        k_msleep(100);
    }
}
