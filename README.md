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
- [ ] implement configurations
- [ ] Dual build, ota and non ota
- [ ] Done

## Issues:

- [ ] pairing does not work after a reboot
- [ ] fix bluetooth autoconnect

## Build Locally

Clone to device

Zephyr SDK: 0.17.0  
Zephyr: V4.0.99

Specify board root as this repository: `-DBOARD_ROOT=/revolute-firmware-ci`

Build for the board 'revolutewireless'

## Download Build from GitHub Actions
 
On the GitHub website, choose Actions, choose the branch, click into the build, under Artifacts, download Zephyr hex or equivalent.

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
