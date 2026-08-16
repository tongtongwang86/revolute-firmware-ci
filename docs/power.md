# Power

The device has four run states plus a true off state. `power_status`
(`enum power_type`, `power.h`) holds the current one, and the sensor thread is
what moves between them.

## States

| State | Sensor rail | i2c0 | Sample period | LED | Entered when |
| --- | --- | --- | --- | --- | --- |
| `PWR_ON` | on | active | 10 ms | slow breathing | Magnet present and connected |
| `PWR_HOLD` | on | active | 100 ms | dim, steady | No movement for 5 s |
| `PWR_STANDBY` | **off** | **suspended** | 2 s | dim / breathing | No magnet, or not connected |
| `PWR_OFF` | off | suspended | — | off | Long press, auto-off, or flat battery |

### Transitions

```
                    magnet + connected
        ┌──────────────────────────────────────┐
        v                                      │
    PWR_ON ──── idle 5 s ────> PWR_HOLD        │
        │  <─── movement ─────────┘            │
        │                                      │
        └── no magnet / disconnected ──> PWR_STANDBY
                                              │
                            long press / auto-off / flat
                                              v
                                          PWR_OFF  (System OFF)
                                              │
                                        button press
                                              v
                                           reset
```

`PWR_STANDBY` is a polling state: every 2 s it brings the rail and bus back up,
takes one sample, and either promotes to `PWR_ON` or drops straight back down.
Bringing the rail up also reconfigures the sensor and discards the stale
rotation reference, so waking never produces a spurious tick.

`PWR_HOLD` deliberately keeps the rail up and only slows the sample rate.
Cycling a rail at 10 Hz costs more than it saves once the sensor reconfiguration
traffic is counted.

## Auto power-off

An inactivity timer (`power_off_work` in `power.c`) shuts the device down. Which
timer applies depends on the link state:

| Situation | Timer | Default |
| --- | --- | --- |
| Connected (`advertising_status == ADV_NONE`) | `timer.autoofftimer` | 10 min |
| Advertising | `timer.autoFilterOffTimer` | 5 min |

Both are in milliseconds, are writable over the configuration service, and
persist to flash. A configured value of `0` falls back to the 10 minute
built-in default rather than disabling shutdown.

The timer is rescheduled from `track_rotation()` whenever ticks are emitted,
throttled to at most once every 5 s so a fast spin does not flood the workqueue
with several hundred reschedules a second.

## Shutdown sequence

`power_off()` runs in a fixed order, and the order matters — steps 2 and 3 need
a working I²C bus, which step 5 takes away.

1. **`pwmled_shutdown()`.** Aborts the LED thread *first* so nothing races for
   the duty cycle, ramps down from the current brightness, writes 0, then
   suspends the PWM device so pinctrl parks P0.29. The nRF52 retains GPIO output
   state through System OFF, so whatever is latched here is what the LED holds
   for the entire time the device is meant to be off.
2. **Stop the bus users.** `sensor_stop()` (which also puts the TLV493D into
   power-down mode), `battery_stop()`, `charger_stop()`.
3. **`battery_shutdown()`.** Runs the BQ27427's shutdown-mode sequence via
   `pm_device_action_run(bq, PM_DEVICE_ACTION_TURN_OFF)`. The gauge is wired
   directly to the cell, so in its normal operating mode it keeps drawing its
   operating current — on the order of 100 µA — no matter what the rest of the
   board does.
4. **Radio and button.** `disable_bluetooth()`, `button_uninit()`.
5. **Cut the rail, then the pins.** `sensor_rail_set(false)` followed by
   suspending i2c0, which applies the `i2c0_sleep` pinctrl state and disconnects
   P0.15/P0.17. Suspending the bus matters as much as cutting the rail: leaving
   the pins driven into a sensor whose supply has just gone away pushes current
   through its ESD clamps into the dead rail.
6. **Wait for release.** Shutdown is usually triggered by a long press, so the
   button is still held. A level-triggered wake source armed while the button is
   down would fire the instant we enter System OFF. `wait_for_button_release()`
   polls for up to 5 s.
7. **Arm and go.** Configure P0.31 as a level-active interrupt, then
   `sys_poweroff()`.

## Waking up

The only wake source is the button. System OFF exit is a reset, so the device
comes back through `main()` with no retained state beyond what is in flash.

One consequence of step 3: the fuel gauge comes back from shutdown only when
`PM_DEVICE_ACTION_RESUME` pulses its GPOUT pin, which `batteryUpdateThread()`
does once at boot. Shutdown also drops the gauge's learned state, so the first
state-of-charge reading after a power cycle is coarse until it settles.

## Active-current notes

- REG1 runs in DC/DC mode (`regulator-initial-mode = <NRF5X_REG_MODE_DCDC>` on
  `&reg1`). The nRF52 SoC init applies this directly from the devicetree
  property; no Kconfig symbol is involved. This is worth roughly a factor of two
  on active current, and depends on the DC/DC inductor being populated.
- The watchdog is configured with `WDT_OPT_PAUSE_IN_SLEEP`. Without it the
  counter runs through idle and the 1 s feed becomes a mandatory wakeup the
  device can never skip.
- The LED thread backs off from 50 ms to 250 ms once the animation has settled
  on a constant target. Breathing patterns keep the fast tick.
- The charger thread only polls while the bus is up (`PWR_ON` or `PWR_HOLD`) and
  takes `i2c_bus_lock()` before doing so.

## Measuring

The reported figures above are design intent, not bench results. When
characterising System OFF current, take the fuel gauge out of the picture first
(hold it in shutdown, or desolder it) — at ~100 µA it will otherwise dominate
any measurement of everything else combined.
