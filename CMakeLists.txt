cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(Revolute)


add_subdirectory(src)


zephyr_include_directories(src)
zephyr_include_directories(config)