# Revolute Wireless Firmware Continuous Integration using GitHub Actions

## New Firmware Roadmap

- [x] Get GitHub Action to build firmware
- [x] Bluetooth Peripheral sample app
- [ ] Baisc bluetooth characteristics test
- [ ] Bluetooth OTA Firmware update
- [ ] New Bluetooth API to send and receive configurations
- [ ] Power state implementations
- [ ] Sensor poll rate testing
- [ ] Combine together
- [ ] Done

## Build Locally

Clone to device

Zephyr SDK: 0.17.0  
Zephyr: V4.0.99

Specify board root as this repository: `-DBOARD_ROOT=/revolute-firmware-ci`

Build for the board 'revolutewireless'

## Download Build from GitHub Actions
 
On the GitHub website, choose Actions, choose the branch, click into the build, under Artifacts, download Zephyr hex or equivalent.



## Resources

MCUMGR, MCUBOOT, OTA-  
https://docs.zephyrproject.org/latest/build/signing/index.html
https://docs.zephyrproject.org/latest/samples/subsys/mgmt/mcumgr/smp_svr/README.html#smp-svr
https://github.com/zephyrproject-rtos/zephyr/tree/main/samples/subsys/mgmt/mcumgr/smp_svr
https://github.com/zephyrproject-rtos/mcumgr/blob/master/README-zephyr.md
https://docs.zephyrproject.org/latest/samples/sysbuild/with_mcuboot/README.html