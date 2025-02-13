
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdlib.h>  // For atoi()
#include <ble.h>
#include <power.h>
#include <pwmled.h>
#include <batterylvl.h>
#include <magnetic.h>

#define THREAD_STACK_SIZE 1024
#define THREAD_PRIORITY 5
#define BUFFER_SIZE 64

#if IS_ENABLED(CONFIG_LOG) //only include debug console if console is enabled

LOG_MODULE_REGISTER(cdc_acm_read, LOG_LEVEL_DBG);

K_THREAD_STACK_DEFINE(console_thread_stack, THREAD_STACK_SIZE);
static const struct device *cdc_acm_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

struct k_thread console_thread_data;

// Function to convert enum value to string
const char* advertising_status_to_string(enum advertising_type status) {
    switch (status) {
        case ADV_NONE:   return "ADV_NONE";
        case ADV_FILTER: return "ADV_FILTER";
        case ADV_CONN:   return "ADV_CONN";
        default:         return "UNKNOWN";
    }
}

void process_command(const char *cmd) {
    char *open_paren = strchr(cmd, '(');
    char *close_paren = strchr(cmd, ')');

    // if (open_paren && close_paren && close_paren > open_paren) {
    //     *open_paren = '\0';  // Split command name
    //     open_paren++;  // Move to argument
    //     *close_paren = '\0';  // Null-terminate argument

    //     int arg = atoi(open_paren);  // Convert argument to integer

    //     if (strcmp(cmd, "damping") == 0) {
    //         damping_b = arg;
    //         LOG_INF("damping_b: %d", arg);
    //     } else if (strcmp(cmd, "center") == 0) {
    //         center = arg;
    //         LOG_INF("center: %d", arg);
    //     } else if (strcmp(cmd, "spring") == 0) {
    //         spring_k = arg;
    //         LOG_INF("spring_k: %d", arg);
    //     } else if (strcmp(cmd, "mass") == 0) {
    //         mass = arg;
    //         LOG_INF("mass: %d", arg);
    //     } else {
    //         LOG_INF("Unknown command: %s", cmd);
    //     }
    // } else 
    if (strcmp(cmd, "remove_bonded_device") == 0) {
        remove_bonded_device();
    } else if (strcmp(cmd, "active_profile_bonded") == 0) {
        if (active_profile_bonded()) {
            printf("device IS bonded\n");
        } else {
            printf("device is NOT bonded\n");
        }
    } else if (strcmp(cmd, "active_profile_connected") == 0) {
        if (active_profile_connected()) {
            printf("device IS connected\n");
        } else {
            printf("device is NOT connected\n");
        }
    } else if (strcmp(cmd, "hi") == 0) {
        LOG_INF(":>");
    } else if (strcmp(cmd, "magnet_strength") == 0) {
        LOG_INF("strength: %d", get_magnet_strength());
    } else if (strcmp(cmd, "adv_status") == 0) {
        LOG_INF("Advertising Status: %s", advertising_status_to_string(advertising_status));
    } else if (strcmp(cmd, "poweroff") == 0) {
        power_off();
    } else if (strcmp(cmd, "update_advertising") == 0) {
        update_advertising();
    } else {
        LOG_INF("Unknown command: %s", cmd);
    }
}

void console_reader_thread(void *p1, void *p2, void *p3) {
    int ret;
    uint8_t buffer[BUFFER_SIZE];
    int buf_pos = 0;

    if (!device_is_ready(cdc_acm_dev)) {
        LOG_ERR("CDC ACM device not ready");
        return;
    }


    while (true) {
        uint8_t c;
        int bytes_read = uart_fifo_read(cdc_acm_dev, &c, 1);

        if (bytes_read > 0) {
            if (c == '\r' || c == '\n') {  // Enter key pressed
                if (buf_pos > 0) {
                    buffer[buf_pos] = '\0';  // Null-terminate string
                    LOG_INF("Received command: %s", buffer);
                    process_command((char *)buffer);
                    buf_pos = 0;  // Reset buffer
                }
            } else if (buf_pos < BUFFER_SIZE - 1) {
                buffer[buf_pos++] = c;  // Store character in buffer
            }
        }

        k_sleep(K_MSEC(10));
    }


}

static int debug_init(void) {
    k_thread_create(&console_thread_data, console_thread_stack, THREAD_STACK_SIZE,
                    console_reader_thread, NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);
    return 0;
}

SYS_INIT(debug_init, APPLICATION, 50);

#endif