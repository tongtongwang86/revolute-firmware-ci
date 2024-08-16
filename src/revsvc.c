#include "revsvc.h"
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(RevSVC, LOG_LEVEL_DBG);

static bool notify_mysensor_enabled;


static void rev_ccc_gyro_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	notify_mysensor_enabled = (value == BT_GATT_CCC_NOTIFY);
}



BT_GATT_SERVICE_DEFINE(rev_svc,
    BT_GATT_PRIMARY_SERVICE(REV_SVC_UUID),

    BT_GATT_CHARACTERISTIC(REV_GYRO_UUID, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_ENCRYPT, NULL, NULL,NULL),

	BT_GATT_CCC(rev_ccc_gyro_cfg_changed,BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),

);

int rev_send_gyro(float r, float i, float j, float k)
{
    if (!notify_mysensor_enabled) {
        return -EACCES;
    }

    // Convert floats to a 32-bit representation if needed.
    // For this example, we'll assume you want to send them as raw 32-bit floats.
    uint32_t quat_data[4];
    memcpy(&quat_data[0], &r, sizeof(float));
    memcpy(&quat_data[1], &i, sizeof(float));
    memcpy(&quat_data[2], &j, sizeof(float));
    memcpy(&quat_data[3], &k, sizeof(float));

    // Send the quaternion data over Bluetooth
    return bt_gatt_notify(NULL, &rev_svc.attrs[2], &quat_data, sizeof(quat_data));
}