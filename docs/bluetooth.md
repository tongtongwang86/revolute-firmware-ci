# Bluetooth

The device is a BLE peripheral exposing three services: HID over GATT, Battery,
and a vendor configuration service.

## Advertising and bonding

`ble.c` runs a three-state advertising machine (`enum advertising_type`):

| State | Meaning |
| --- | --- |
| `ADV_CONN` | Open advertising — no bonds exist, anyone may pair |
| `ADV_FILTER` | Filtered advertising — bonded but not connected, accept-list only |
| `ADV_NONE` | Connected, or otherwise not advertising |

`update_advertising()` picks the target state from whether a bond exists and
whether a connection is live, and is always invoked from the system workqueue —
calling it directly from a connection callback would still see the outgoing
connection as active.

`advertising_status == ADV_NONE` is used throughout the firmware as the
shorthand for "we have a host", including by the sensor state machine and the
auto-off timer.

Triple-clicking the button calls `remove_bonded_device()`, which unpairs
everything and returns to open advertising.

## Connection parameters

Each detent produces a press *and* a release, so the notification rate is twice
the detent rate. At 30 detents per revolution a brisk spin needs several hundred
notifications per second, and the link can only carry a few packets per
connection event — the connection interval is therefore a hard cap on how fast
the wheel can be turned before reports start queueing.

The firmware asks for 7.5–15 ms:

- `CONFIG_BT_PERIPHERAL_PREF_MIN_INT=6` / `MAX_INT=12` in `prj.conf`. A
  `min == max` range is commonly rejected outright, so a range is advertised
  rather than a single value.
- `connected()` schedules an explicit `bt_conn_le_param_update()` 500 ms after
  the link comes up. Some centrals reject a parameter update sent immediately
  on connect.
- `le_param_updated()` re-requests if the granted interval is still slower than
  15 ms, at most twice per connection. Some centrals — macOS in particular for
  HID — will never grant a shorter interval, and repeatedly asking just burns
  airtime.

If input feels like it lags or drops only at speed, check the negotiated
interval reported by `le_param_updated()` first.

## HID over GATT

`hog.c` publishes one HID service with four input reports:

| Report ID | Collection | Attribute index (`transport`) | CCC index |
| --- | --- | --- | --- |
| 1 | Keyboard | 5 | 7 |
| 2 | Consumer Control | 9 | 11 |
| 3 | Mouse | 13 | 15 |
| 4 | Game Pad | 17 | 19 |

The **attribute index is the value stored in `config.up_transport` /
`config.dn_transport`** — the configuration protocol names a report by its
position in the GATT service, not by its report ID. `transport_to_report()`
validates this: only 5, 9, 13 and 17 are accepted, which also bounds the array
index used against `hog_svc.attrs[]`.

Each report tracks its own subscription state (`notify_enabled[]`) through its
own CCC callback. A host that unsubscribes from one report no longer silences
the others.

All reports are 8 bytes:

- **Keyboard**: `[modifiers, reserved, key1..key6]`
- **Mouse**: `[buttons, X, Y, wheel, AC Pan, ...]`
- **Consumer / Gamepad**: usage array

### The send pipeline

```
sensor thread ──> revolute_submit() ──> k_msgq (32 entries) ──> sender thread (prio 3)
```

A queue entry is a `struct hid_revolute_tick`: the target transport, the eight
report bytes, and a `release` flag. Three properties follow from that shape and
each of them fixes a real failure mode:

- **Press and release are one entry.** They are dequeued and sent together, so a
  release can never be dropped independently of its press. That is what used to
  leave a key held down until the next tick.
- **Report bytes are captured at detection time**, not read from a shared global
  when the report is eventually transmitted. Continuous-mode reports carry the
  magnitude that was measured, not whatever the sensor thread had overwritten it
  with by then.
- **`bt_gatt_notify()`'s return code is checked.** `-ENOMEM`, `-EAGAIN` and
  `-ENOBUFS` mean the controller's TX buffers are momentarily full — exactly what
  a fast spin causes — and are retried up to ten times with a 2 ms backoff.
  Other errors (not connected, not subscribed) return immediately.

If the queue does overflow, the **oldest** entry is discarded so the queue keeps
tracking the most recent motion, and `revolute_dropped_tick_count()` counts it.

The relevant buffer settings in `prj.conf` are `CONFIG_BT_BUF_ACL_TX_COUNT=10`
and `CONFIG_BT_ATT_TX_COUNT=10`.

### Discrete vs continuous

`is_discrete()` decides which mode a direction uses:

- Keyboard (5) and Consumer (9) are always discrete.
- Mouse (13) is discrete only if report bytes 2, 3 and 4 are all zero, i.e. a
  button-only report. If any of Y, wheel or AC Pan is nonzero, the report is
  treated as continuous.

  Note the asymmetry: byte 1 (mouse X) is **not** examined, so a template that
  moves only along X is classified as discrete and will be sent as a fixed
  press/release pair rather than a relative movement. That is long-standing
  behaviour, not a recent change; check it before configuring an X-axis mapping.

In continuous mode the configured report acts as a template and the **first
nonzero byte** is replaced with a signed magnitude in whole degrees, clamped to
±127. So a wheel template of `{0,0,0,1,0,0,0,0}` sends the accumulated rotation
in the wheel byte.

## Configuration service

Vendor service `00001523-1212-efde-1523-785feabcd133`. All characteristics
require an encrypted link.

| UUID suffix | Access | Payload |
| --- | --- | --- |
| `…1524` | Notify | `rev_stats_t` — quaternion + rotation value |
| `…1525` | Read | `rev_config_t` |
| `…1526` | Write | `rev_config_t` |
| `…1527` | Write | Device name, UTF-8, < 32 bytes |
| `…1528` | Read / Write | `rev_timer_t` |

### `rev_config_t`

```c
typedef struct {
    uint8_t deadzone;        // degrees; floor on detent size
    uint8_t up_report[8];    // HID report sent on clockwise detents
    uint8_t up_identPerRev;  // detents per revolution, clockwise
    uint8_t up_transport;    // 5 / 9 / 13 / 17
    uint8_t dn_report[8];    // HID report sent on counter-clockwise detents
    uint8_t dn_identPerRev;  // detents per revolution, counter-clockwise
    uint8_t dn_transport;    // 5 / 9 / 13 / 17
} rev_config_t;
```

Defaults live in `statemanager.c`: 30 detents per revolution in both directions,
keyboard transport.

Writing this characteristic recalculates both detent steps
(`set_cw_identsperrev()` / `set_ccw_identsperrev()`) and persists to flash
immediately. The two directions keep independent step sizes.

### `rev_timer_t`

```c
typedef struct {
    uint32_t autoofftimer;       // ms; inactivity shutdown while connected
    uint32_t autoFilterOffTimer; // ms; inactivity shutdown while advertising
} rev_timer_t;
```

See [power.md](power.md) for how these are applied.

### Persistence

`settings.c` stores both structures under the `rev_module` settings subtree:

| Key | Contents |
| --- | --- |
| `rev_module/config` | `rev_config_t` |
| `rev_module/rev_timer` | `rev_timer_t` |

`save_config()` writes both. Loading happens through `settings_load()` during
Bluetooth initialisation, alongside the bond data.

## Button

Handled in `button.c`, debounced at 50 ms:

| Gesture | Action |
| --- | --- |
| Single click | Logged only |
| Double click | Logged only |
| Triple click | Unpair all bonds and re-advertise |
| Hold ≥ 700 ms | Power off |

The click window is 400 ms.

## Debugging

`prj.conf` resolves to `CONFIG_LOG=n` with `CONFIG_PRINTK=y`, so every `LOG_*`
call — including the dropped-tick and notify-failure warnings — is compiled out
in the shipping configuration, while `printk()` survives. Turn `CONFIG_LOG=y`
and a backend on for bench work.
