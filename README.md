# Revolute Wireless Firmware Continuous Integration using GitHub Actions

## New Firmware Roadmap

- [x] Get GitHub Action to build firmware
- [x] Bluetooth Peripheral sample app
- [x] Bluetooth OTA Firmware update
- [x] Bluetooth Connection and Bonding
- [x] Baisc bluetooth characteristics test
- [x] New Bluetooth API to send and receive configurations
- [x] Settings to save and load revolute configurations
- [x] button hold time, double click, and triple click implementations
- [x] Power state implementations
- [x] fuel gauge over bluetooth
- [x] Sensor poll rate testing
- [x] magnetic sensor polling
- [x] Dual build, ota and non ota
- [x] Auto power off
- [x] Implement configurations
- [x] Done

## Issues:

- [ ] fix bluetooth autoconnect after disconnect
- [ ] fix revolute not working on iphone, ipad, vision pro

## Build Locally

Clone to device

Zephyr SDK: 0.17.0  
Zephyr: V4.0.99

Specify board root as this repository: `-DBOARD_ROOT=/revolute-firmware-ci`

Enable sysbuild  

Use prj.conf  

Build for the board 'revolutewireless'

## Build development (No OTA)

Build board without sysbuild using the development.conf

## Download Build from GitHub Actions
 
In this github repo => <a href = "https://github.com/tongtongwang86/revolute-firmware-ci/actions">Actions</a> => click into a build => Artifacts

- Bootloader - OTA bootloader, must flash
- RevoluteHex - Initial OTA firmware, must flash
- RevoluteBin - OTA update file, upload wirelessly using nrfconnect mobile app


## Initial flash revolute board

Upload the bootloader.hex first with full chip erase  
Upload the revolutefirmware.hex second without chip erase

## Wireless firmware upgrades

After having completed initial flash, you may wirelessly flash revolute.bin using mcumgr compatible ota apps such as nrf connect or revolute configurator

## Resources

MCUMGR, MCUBOOT, OTA-  
https://docs.zephyrproject.org/latest/build/signing/index.html
https://docs.zephyrproject.org/latest/services/device_mgmt/mcumgr.html
https://docs.zephyrproject.org/latest/samples/subsys/mgmt/mcumgr/smp_svr/README.html#smp-svr
https://github.com/zephyrproject-rtos/zephyr/tree/main/samples/subsys/mgmt/mcumgr/smp_svr
https://github.com/zephyrproject-rtos/mcumgr/blob/master/README-zephyr.md
https://docs.zephyrproject.org/latest/samples/sysbuild/with_mcuboot/README.html
