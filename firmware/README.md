# Firmware

Place the current Arduino sketch in this folder with the filename:

```text
CardputerAirMonitor.ino
```

## Demo Mode

Demo Mode works without an API token.

The source can keep:

```cpp
const char* API_TOKEN = "PASTE_YOUR_TOKEN_HERE";
```

and the built-in simulated scenarios can still be used.

## Live Mode

For real alerts:

1. Request your own API token from the alerts.in.ua developers.
2. Insert it into `CardputerAirMonitor.ino`.
3. Compile the sketch.
4. Flash the firmware to the Cardputer.

The token is inserted **before flashing**; it is not entered later on the Cardputer keyboard.

## Before publishing to GitHub

Make sure the public source contains:

```cpp
const char* API_TOKEN = "PASTE_YOUR_TOKEN_HERE";
```

and not your real API token.
