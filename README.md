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

- [ ] revolute keys not working on ipad and vision pro
- [ ] bluetooth not working after extended time even though revolute is still on
- [ ] fix bluetooth autoconnect after disconnect


## Documentation

- [docs/architecture.md](docs/architecture.md) — modules, threads, rotation tracking
- [docs/power.md](docs/power.md) — run states, auto-off, shutdown sequence
- [docs/bluetooth.md](docs/bluetooth.md) — HID reports, the send pipeline, configuration service

## Build Locally

Requirements:

- Zephyr SDK 0.17.0
- Zephyr 4.1.99 (upstream `main`)

Two board targets are supported:

```sh
# The product board
west build -b revolutewireless --sysbuild -- -DBOARD_ROOT=$PWD

# Nordic devkit; picks up nrf52833dk_nrf52833.overlay automatically
west build -b nrf52833dk/nrf52833
```

`BOARD_ROOT` must point at this repository so the `boards/revolutewireless`
definition is found.

### Gotchas

- **No spaces in the checkout path.** Zephyr's Kconfig step splits the
  application path on whitespace and will fail with a truncated
  "File not found" error. If your working copy lives somewhere with a space in
  the name, build through a symlink:

  ```sh
  ln -s "/path/with a space/revolute-firmware-ci" /tmp/revolute
  west build -b revolutewireless -d /tmp/build /tmp/revolute -- -DBOARD_ROOT=/tmp/revolute
  ```

- If `west` fails with `ModuleNotFoundError`, its Python environment is missing
  Zephyr's build dependencies (`pykwalify`, `packaging`, `pyelftools`). Install
  `zephyr/scripts/requirements-base.txt` into whichever interpreter west uses.

- The build **is** a sysbuild + MCUboot build (`sysbuild.conf`,
  `sysbuild/mcuboot.conf`), so `west build` needs `--sysbuild` and the
  workspace needs `bootloader/mcuboot` checked out.

- The app image is a tight fit in slot0: 203 KB of the 220 KB usable. That is
  why `prj.conf` selects picolibc — newlib costs ~22 KB more and overflows the
  slot. Watch this number when adding features.

- MCUboot is built with the console off (see the comment in
  `sysbuild/mcuboot.conf`); the board's `zephyr,console` is a USB CDC ACM node
  that the bootloader never instantiates.

### Logging

`prj.conf` builds with logging disabled (`CONFIG_LOG=n`, `CONFIG_PRINTK=y`).
For bench work, enable a backend:

```sh
west build -b revolutewireless -- -DBOARD_ROOT=$PWD \
    -DCONFIG_LOG=y -DCONFIG_LOG_BACKEND_UART=y -DCONFIG_SERIAL=y
```

Unlike `main`, this branch has no `development.conf`.

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
