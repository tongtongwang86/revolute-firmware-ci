# Revolute Wireless Firmware Continuous Integration using github actions.

## New firmware roadmap

- [ ] Bluetooth OTA Firmware update
- [ ] New Bluetooth API to send and recieve configurations
- [ ] Power state implementations
- [ ] Sensor Poll rate testing 
- [ ] Combine together 
- [ ] Done

## Build Locally

Clone to device


Zephyr SDK : 0.17.0  
Zephyr : V4.0.99

Specify board root as this repository: -DBOARD_ROOT=/revolute-firmware-ci

## Download Build from github Actions

On the github website, choose actions, choose the branch, click into the build, under artifacts, download Zephyrhex or equivalent 