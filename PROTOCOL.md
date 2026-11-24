# Daewoo AC UART protocol

The indoor unit's main board talks to its WiFi module (originally a Tuya module) over UART:
**9600 baud, 8N1**. This protocol is **not** the documented Tuya MCU protocol (`55 AA ...`).
It is a fixed-layout, single-header protocol. As far as we could find, nobody else has
published it. Similar "Tuya module, custom protocol" ACs (Electriq, TCL/Pioneer `BB`, JHS
`A5`, Hisense `F4 F5`) all use different framing.

Everything below was found by capturing frames and by writing test values and reading back
what the AC kept (see [How this was decoded](#how-this-was-decoded)). Fields marked
*unknown* are stored by the AC, but we haven't seen what they do.

## Framing

```
AA <len> <op> <payload ...> <checksum>
```

| Byte | Meaning |
|------|---------|
| `AA` | Header, both directions |
| `len` | Number of bytes after this one, including the checksum (`0x02` for a poll, `0x14` for a full frame) |
| `op` | `0x01` status/read, `0x02` write |
| checksum | Sum of all previous bytes in the frame, modulo 256 |

### Messages

| Direction | Frame | Notes |
|-----------|-------|-------|
| Module → AC | `AA 02 01 AD` | Status poll, sent every ~2 s |
| AC → Module | `AA 14 01 <19 bytes> <chk>` | Status report (22 bytes) |
| Module → AC | `AA 14 02 <19 bytes> <chk>` | Write: the complete desired state, same layout as the status report |
| AC → Module | `AA 14 02 ...` | Write acknowledgement. Not a full status: `current_temperature` is `0` and some flags read `0`, so it shouldn't be parsed as state. |

Each write carries the **whole** state. Start from the last status report and change only
the fields you mean to change, so the unknown fields go back to the AC unchanged.

## Payload byte map

Index = position after `AA 14` (the `op` byte is index 0). Matches `struct DaewooState`
in `components/daewoo_ac/daewoo_ac.h`.

| Idx | Field | Values |
|-----|-------|--------|
| 0 | `operation` | `0x01` status, `0x02` write / ack |
| 1 | `power_state` | `0x00` off, `0x01` on |
| 2 | `ext_flags` | bits 2–4 (`0x1C`): 3-bit value 0–7, *unknown*; bit 6 (`0x40`): *unknown*; other bits ignored. **Only accepted while the unit is running**: writes are dropped when it is off. |
| 3 | `vertical_vane` | `00` swing, `01` down, `02` medium-down, `03` medium, `04` up-medium, `05` up, `06` static |
| 4 | `flags0` | `0x02` horizontal swing; `0x01` *unknown* (stored, no visible effect); `0x04` and `0x10` observed always set; `0x08`/`0x20`/`0x40`/`0x80` ignored |
| 5 | `mode` | `00` auto, `01` cool, `02` dry, `03` heat, `04` fan only |
| 6 | `flags1` | `0x01` quiet, `0x02` UV light / ionizer, `0x10` display; `0x04` *unknown* (stored, no visible effect); `0x08`/`0x20`/`0x40`/`0x80` ignored |
| 7 | `fan_mode` | `00` auto, `01` low, `02` medium, `03` high |
| 8 | `target_temperature` | °C, 16–32 |
| 9 | `current_temperature` | °C, read only |
| 10 | `timer_mode` | low 2 bits only. `00` none, `02` **off-timer** (confirmed with the IR remote), `01` presumably on-timer (not confirmed). The display shows the clock icon when non-zero. |
| 11 | `timer_hours` | `01`–`17` hex = 1–23 h, `00` = 24 h (the remote offers 24 steps). Ignored unless `timer_mode` is set. |
| 12 | `timer_counter` | read only, *unknown*. Went `06 → 07 → 08` about once a minute while an off-timer ran, then stayed at `07` after the timer was cancelled. Writes are ignored. |
| 13–18 | `reserved` | always `00`; writes are ignored |
| 19 | `checksum` | see framing |

### Example

Unit running in fan-only mode, vertical vane static, 5 h off-timer:

```
AA 14 01 01 00 06 14 04 10 01 18 1B 02 05 06 00 00 00 00 00 00 2F
      |  |  |  |  |  |  |  |  |  |  |  |  |
      |  |  |  |  |  |  |  |  |  |  |  |  timer_counter 06
      |  |  |  |  |  |  |  |  |  |  |  timer_hours 5
      |  |  |  |  |  |  |  |  |  |  timer_mode 02 (off-timer)
      |  |  |  |  |  |  |  |  |  current 27 °C
      |  |  |  |  |  |  |  |  target 24 °C
      |  |  |  |  |  |  |  fan low
      |  |  |  |  |  |  flags1 0x10 (display on)
      |  |  |  |  |  mode 04 fan only
      |  |  |  |  flags0 0x14 (horizontal swing off)
      |  |  |  vertical vane 06 static
      |  |  ext_flags 00
      |  power on
      op 01 status
```

## Horizontal vane: swing on/off only

There is **no horizontal vane position field**. Evidence:

- The IR remote only has horizontal swing on/off.
- With the unit running, no write to any byte moved the horizontal vane. That includes every
  `ext_flags` value, the unknown `flags0`/`flags1` bits, and bytes 10–18.
- When swing is turned off, the vane stops wherever it is.
- The Tuya data model for a Daewoo split ([tuya-local `confortotal_minisplit.yaml`],
  product `keyxkdt8t38pvrkk`, Daewoo DA-12XMSAC23) also exposes horizontal swing as a plain
  boolean (DP 33), while vertical swing has positions (DP 31: `off`, `same`, `vane_1`…`vane_5`).

## Still unidentified

- `ext_flags` 3-bit field and bit `0x40`. During probing, the vertical vane moved for some of these values even though `vertical_vane` kept reading `06`. Which values did it wasn't recorded.
- `flags0` bit `0x01` and `flags1` bit `0x04`.
- `timer_counter`.
- Whether `timer_mode = 01` is the on-timer.

Candidates, from the features the Tuya data model lists for this unit family: sleep (DP 25),
emergency heat (DP 12), ionizer (DP 11), a "strong"/turbo fan mode, and the on-timer
(DP 101/103).

## How this was decoded

1. **Passive capture**: log every received status frame (the component logs them at INFO), then change
   one setting with the IR remote and diff the frames. The timer fields came from stepping the remote's
   off-timer through all 24 values and cancelling it.
2. **Write-and-read-back**: send a write frame that flips a single bit in the last status, wait for the
   next two status reports, and check whether the AC kept the bit. Restore it immediately if it did.
   Do this **with the unit running** (fan only is safe). With the unit off, the AC drops writes to
   `ext_flags` and the timer bytes, so they look unused.

[tuya-local `confortotal_minisplit.yaml`]: https://github.com/make-all/tuya-local/blob/main/custom_components/tuya_local/devices/confortotal_minisplit.yaml
