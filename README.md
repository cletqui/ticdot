# pebble-face

Minimal analog watchface for Pebble Time 2, written in C.

## Features

- Orange hour hand, white minute hand — no tick marks
- Single dot at 12 o'clock — turns red on Bluetooth disconnect + vibration
- Ten battery dots along the bottom arc (light gray filled, dark gray empty)
- Updates every minute (`MINUTE_UNIT`) for maximum battery efficiency

## Building & running

```sh
pebble build
pebble install --emulator emery    # Pebble Time 2 emulator
pebble install --phone <ip>        # sideload via the Pebble app (Developer Mode)
```

## Documentation

Full SDK docs, tutorials, and API reference: <https://developer.rebble.io>
