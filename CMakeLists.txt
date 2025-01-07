
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(Revolute)

# Verify that this samnple is built through sysbuild to ensure MCUboot is
# automatically included and that sample specific MCUboot configurations are
# used when building MCUboot.
test_sysbuild()
add_subdirectory(src)


target_sources(app PRIVATE src/main.c)
