#
# Copyright (c) 2023 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_sources(app PRIVATE main.c ble.c gpio.c hog.c batterylevel.c revsvc.c sensor_poll.c)
target_include_directories(app PRIVATE .)
