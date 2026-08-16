# Architecture

Revolute is a rotary input device: a magnet on the shaft, a 3D Hall sensor
watching it, and a Bluetooth HID keyboard/consumer/mouse/gamepad on the other
end. This document describes how the firmware is put together on the
`newdevelopment` branch.

## Hardware it runs on

| Part | Bus / pin | Notes |
| --- | --- | --- |
| nRF52833 (QFN40) | — | REG1 runs in DC/DC mode, set via `&reg1` in the board DTS |
| TLV493D 3D Hall sensor | i2c0 `0x5E` | On the **switched** rail (`MOS_VDD`) |
| BQ27427 fuel gauge | i2c0 `0x55` | Powered directly from the cell — always on |
| BQ25180 charger | i2c0 `0x6A` | |
| i2c0 | SDA P0.17, SCL P0.15 | `i2c0_default` / `i2c0_sleep` pinctrl states |
| Sensor rail enable | P0.11 (`mosfet` alias) | Q5 is a **P-channel high-side** switch |
| Status LED | P0.29 via `pwm0` | |
| Button | P0.31 (`sw0` alias) | Pull-up, active low; also the System OFF wake source |

The sensor rail deserves a note, because the polarity looks inverted at first
glance. Q5 is a PMOS high-side switch: pulling its gate **low** turns the rail
**on**. The devicetree declares the alias `GPIO_ACTIVE_HIGH`, so in code
`gpio_pin_set_dt(&mosfet, 1)` drives the pin high and turns the rail **off**.
That is deliberate and correct — see `sensor_rail_set()` in `power.c`.

## Modules

| File | Responsibility |
| --- | --- |
| `main.c` | Starts the sensor, installs and feeds the watchdog |
| `sensor.c` | TLV493D driver, rotation tracking, and the run-state machine |
| `hog.c` | HID-over-GATT service and the report send pipeline |
| `ble.c` | Advertising, bonding, connection parameters |
| `revsvc.c` | Revolute configuration GATT service |
| `settings.c` | Persisting config and timers to flash |
| `statemanager.c` | The shared `config` / `timer` / `stats` structures |
| `power.c` | Run states, the standby/resume path, and the shutdown sequence |
| `pwmled.c` | Status LED animation |
| `button.c` | Click and long-hold detection |
| `batterylvl.c` | Fuel gauge access and the Battery Service |
| `charger.c` | Charger configuration and status polling |

## Threads

Everything is a long-lived thread; there is no application workqueue beyond
Zephyr's system one.

| Thread | Priority | Period | Purpose |
| --- | --- | --- | --- |
| `main` | 0 | 1 s | Feeds the watchdog |
| HID sender (`hog.c`) | 3 | blocking | Drains the report queue onto the air |
| Sensor (`sensor.c`) | 5 | 10 / 100 / 2000 ms | Samples the Hall sensor, drives run states |
| `rev_svc` | 5 | 100 ms | Streams the stats characteristic when subscribed |
| PWM LED | 6 | 50 / 250 ms | Animates the status LED |
| Charger | coop 7 | 5 s | Logs charger status |
| Battery | 7 | one-shot | Wakes the gauge out of shutdown at boot, then exits |

Anything that needs deferring runs on the system workqueue: advertising
updates, the connection-parameter re-request, button click/hold handling, and
the auto-off timer.

### Startup order

All application modules register with `SYS_INIT(..., APPLICATION, 50)` except
`power.c`, which uses priority 60 so the auto-off timer is armed after the
things it will later shut down exist. `sensor_init()` is called explicitly from
`main()` rather than from a `SYS_INIT`, because it must run after the kernel is
up and it creates the thread that owns the I²C bus.

## Data flow

```
TLV493D ──I²C──> sensor.c            hog.c                     BLE
                 ┌──────────────┐    ┌───────────────────┐
  field vector ─>│ track_rotation│──>│ k_msgq (32 ticks) │──> sender thread ──> bt_gatt_notify()
                 │  atan2 integr.│   └───────────────────┘         │
                 └──────────────┘                                  └─ retries on -ENOMEM
```

### Rotation tracking

The TLV493D reports a magnetic field **vector**, not an absolute shaft angle.
Rotation is therefore integrated rather than read: each sample is normalised to
a unit vector, and the signed angle from the previous sample to the current one
is taken with `atan2f`. That angle accumulates in `angle_accum`, and whenever
the accumulator passes the per-direction detent size a tick is emitted and the
step is subtracted.

Two consequences worth knowing:

- The measurement is unambiguous as long as the shaft turns less than half a
  revolution between samples. At the 10 ms active rate that is roughly 30,000
  RPM of headroom, so direction cannot invert from aliasing at any realistic
  speed. Anything that stalls the sample loop eats into that margin, which is
  why per-sample register dumps and console printing are compiled out by
  default (`SENSOR_DEBUG_DUMP` in `sensor.c`).
- Emission is a `while` loop, not an `if`. A single sample that spans three
  detents emits three ticks.

Because there is no absolute angle, there are no fixed detent boundaries in
space and nothing to "centre" the detents on. The equivalent operation — 
dropping the stale reference vector — happens in `sensor_reinit_after_resume()`
whenever the sensor rail has been cycled.

### Detent size

`step_from_idents()` converts `up_identPerRev` / `dn_identPerRev` into a step in
radians, separately per direction. The configured `deadzone` acts as a floor: no
step is ever smaller than `max(deadzone, 1°)`, so a nonsense configuration
cannot make the wheel chatter.

## I²C ownership

The sensor thread owns i2c0. It is the only thread that calls
`power_standby()` / `power_resume()`, and it performs the periodic fuel-gauge
read itself rather than leaving that to a free-running thread that could hit a
suspended bus.

Any *other* thread touching the bus must hold `i2c_bus_lock()` (declared in
`power.h`), which serialises against the suspend/resume path. Today the charger
thread is the only such caller.

## Build

See the repository README. Two board targets are supported:

```
-DBOARD=revolutewireless -DBOARD_ROOT=<repo>   # the product
-DBOARD=nrf52833dk/nrf52833                    # devkit, uses the .overlay
```

The devkit overlay has no `mosfet` alias; `power.c` compiles the switched-rail
handling out when the alias is absent, so standby simply skips the rail there.
