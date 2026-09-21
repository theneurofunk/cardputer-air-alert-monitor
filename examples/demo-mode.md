# Demo Mode

Demo Mode allows the display, menu and sound behavior to be tested without an API token and without waiting for a real alert.

## No API token required

You can flash the firmware with the default placeholder:

```cpp
const char* API_TOKEN = "PASTE_YOUR_TOKEN_HERE";
```

and still use Demo Mode normally.

## Open Demo Mode

On the Cardputer:

```text
M → DEMO MODE
```

Enable Demo Mode and choose a scenario.

## Included scenarios

### All clear

Both default areas are clear.

### Kyiv: drones

Example:

```text
Kyiv City
ALERT
Since: 20:14
Threat: DRONES
```

### Kyiv: drones + ballistic

Tests multiple simultaneous threat categories.

### Zhytomyr Raion: drones

Tests the second default monitored area.

### Both areas

Tests simultaneous alerts in Kyiv City and Zhytomyr Raion.

### MiG-31K

Tests the MiG-31K threat label and sound behavior.

## Quick cycling

While Demo Mode is active, press:

```text
R
```

on the home screen to move to the next demo scenario.

## What Demo Mode does not test

Demo Mode does not verify:

- Wi-Fi
- API authentication
- real JSON parsing
- real location matching
- upstream API availability

For full operation with real alerts, request your own API token, insert it into the firmware source code, and flash the Cardputer again.

Use **System info** and **LIVE DEBUG** for real API troubleshooting.
