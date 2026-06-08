# pebble-face

Minimal analog watchface for Pebble color watches, written in C.

## Layout

All indicators are dots arranged clockwise around the outer edge:

```
             ●          ← 12 o'clock (bigger dot, red = BT off)
        ○ ○ ○ ○ ○       ← battery (1–2 o'clock, 5 dots × 20%)
   ○                    ← day in binary (10–11 o'clock, 5 dots)
     ○ ○ ○ ○            ←   MSB left, LSB right
                        ←   e.g. day 13 = 01101 = ○●●○●
   ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ← steps (4:30–7:30, 10 dots = daily goal)
```

### Battery (1–2 o'clock) — 5 dots, 10° spacing
Each dot = 20% charge. All white when full, bottom dot red when ≤ 20%.

### Steps (4:30–7:30) — 10 dots, 10° spacing
Each dot = 1/10th of the daily goal (default 10 000 steps). Dimmed gray until reached, white when hit. Cycles to a second color on the next lap, third color on the lap after.

### Day of month (10–11 o'clock) — 5 dots, binary
Reading left → right (9 o'clock → 12 o'clock): bit values **1 · 2 · 4 · 8 · 16** (LSB nearest 9 o'clock, symmetric with battery group). Lit dot = 1, dimmed = 0.

| Day | Binary | Pattern (LSB left) |
|-----|--------|--------------------|
| 1   | 00001  | ● ○ ○ ○ ○ |
| 8   | 01000  | ○ ○ ○ ● ○ |
| 13  | 01101  | ● ○ ● ● ○ |
| 31  | 11111  | ● ● ● ● ● |

## Features

- Updates on `MINUTE_UNIT` only — no per-second wakeups
- Steps polled in the minute tick (no extra health subscription)
- Battery and Bluetooth are event-driven; canvas only redraws when display state changes
- Geometry fully responsive: adapts to basalt (144×168), chalk (180×180 round), emery (200×228)
- Unobstructed area aware (timeline peek support)
- Settings page via Clay (phone app): step goal, over-goal colors, hand colors, vibration toggle, per-indicator visibility

## Platforms

Color platforms only: **basalt**, **chalk**, **emery**.

## Building

```sh
# First time: install Clay
npm install pebble-clay --save --ignore-scripts

pebble build
pebble install --emulator emery      # Pebble Time 2 emulator
pebble install --phone <ip>          # sideload via Developer Mode
pebble install --cloudpebble         # sideload via Rebble proxy
```

> `pebble package install pebble-clay` fails when `npm` is not in the pebble-tool PATH — use `npm install` directly.

## Settings

Configured from the Pebble / Rebble phone app:

| Setting | Default |
|---|---|
| Show date dots | On |
| Show battery dots | On |
| Daily step goal | 10 000 |
| Show step dots | On |
| Hour hand color | Orange |
| Minute hand color | White |
| 1st over-goal color | Orange |
| 2nd over-goal color | Cyan |
| Vibrate on disconnect | On |

Available colors: Orange, Red, Green, Blue, Cyan, Yellow, Magenta, White.

## Documentation

Full SDK docs and API reference: <https://developer.rebble.io>
