# M5Stack Cardputer Air Alert Monitor

![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue)
![Framework](https://img.shields.io/badge/framework-Arduino-teal)
![Device](https://img.shields.io/badge/device-M5Stack%20Cardputer-red)
![License](https://img.shields.io/badge/license-MIT-green)

A compact real-time air-alert monitor for the original **M5Stack Cardputer / StampS3**.

## Screenshots

### Main screen

![Main screen](docs/screenshots/home-page.png)

The main screen shows the current alert state, alert start time and available threat information for the monitored areas.

### Demo Mode

![Demo Mode](docs/screenshots/demo-mode.png)

Demo Mode works without an API token and lets you test the interface, sounds and simulated alert scenarios.

### System diagnostics

![System diagnostics](docs/screenshots/system-info.png)

The diagnostics screen shows battery information, Wi-Fi status, API response status, JSON parsing status and alert counters.
The firmware connects to Wi-Fi and uses the **alerts.in.ua API** to show alert state, alert start time, threat categories, battery information and API diagnostics directly on the Cardputer display.

> **Demo Mode works without an API token.**
>
> For live alerts, the user must request a personal alerts.in.ua API token,
> insert it into `CardputerAirMonitor.ino`, and then flash the firmware to the device.

> This is an independent community project. It is not affiliated with or endorsed by M5Stack or alerts.in.ua.

## Quick Start: Demo Mode or Live Mode

The firmware can be used in two modes.

### Demo Mode — no API token required

You can install and test the firmware without an alerts.in.ua API token.

Demo Mode uses built-in simulated alert scenarios, so you can test:

- the user interface
- alert and all-clear screens
- sound notifications
- alert start time display
- threat-type display
- Kyiv City alerts
- Zhytomyr Raion alerts
- multiple simultaneous threats
- MiG-31K, drones, ballistic and cruise-missile scenarios

After flashing the firmware, open:

```text
M → DEMO MODE
```

and enable one of the available test scenarios.

While Demo Mode is enabled, press:

```text
R
```

on the main screen to cycle through the demo scenarios.

**No API token is required for Demo Mode.**

This is useful for testing the project and learning how it works before requesting API access.

---

### Live Mode — API token required

For full operation with real alert data from alerts.in.ua, you need your own personal API token.

The token must be inserted into the firmware source code **before flashing the firmware to the Cardputer**.

The token is **not** entered later through the Cardputer keyboard.

### How to request an API token

1. Open the alerts.in.ua developer website:

   https://devs.alerts.in.ua/

2. Find the API access / token request section.

3. Submit a request to the alerts.in.ua developers for API access.

You can describe the project like this:

**Project name:**

```text
M5Stack Cardputer Air Alert Monitor
```

**Project description:**

```text
A personal non-commercial IoT device based on the M5Stack Cardputer.
The device displays air raid alert status, alert start time and available
threat information for selected Ukrainian regions using the alerts.in.ua API.
```

**Usage:**

```text
Personal / non-commercial use.
```

4. Wait for the developers to provide your personal API token.

### Insert the token before flashing

Open:

```text
firmware/CardputerAirMonitor.ino
```

Find:

```cpp
const char* API_TOKEN = "PASTE_YOUR_TOKEN_HERE";
```

Replace it with the token you received:

```cpp
const char* API_TOKEN = "your_personal_api_token";
```

Then compile and flash the firmware to the M5Stack Cardputer.

If you later change the API token, update this line and flash the firmware again.

> **Important:** Never publish your real API token on GitHub.
>
> Before uploading the source code to a public repository, change the line back to:
>
> ```cpp
> const char* API_TOKEN = "PASTE_YOUR_TOKEN_HERE";
> ```
>
> Every user should request and use their own API token.

## Features

- Automatic Wi-Fi reconnect
- Wi-Fi network scanner and password entry on the Cardputer
- Saved Wi-Fi settings using ESP32 Preferences
- Multiple monitored areas
- Default monitoring:
  - Kyiv City
  - Zhytomyr Raion
- `ALERT`, `PARTIAL`, and `CLEAR` states
- Alert start time
- Threat categories
- Different sound notifications
- Battery percentage and voltage
- System/API diagnostics
- Demo mode for offline testing
- Live API debug mode
- Manual refresh
- Reboot from the menu

## Threat categories

The firmware currently recognizes:

- Drones
- Ballistic missiles
- Cruise missiles
- Unspecified missiles
- MiG-31K activity
- Strategic aircraft activity
- Tactical aircraft activity
- Guided aerial bombs
- Air-defense activity

Unknown threat values are shown as `UNKNOWN`.

## Hardware

| Component | Hardware |
|---|---|
| Device | Original M5Stack Cardputer |
| MCU | ESP32-S3 / StampS3 |
| Display | Built-in Cardputer LCD |
| Input | Built-in keyboard |
| Network | Wi-Fi |
| Framework | Arduino / C++ |
| Alert data | alerts.in.ua API |

## Requirements

For Demo Mode:

- Original M5Stack Cardputer
- Arduino IDE
- M5Cardputer library
- ArduinoJson library

For Live Mode you additionally need:

- Wi-Fi connection
- Your own personal alerts.in.ua API token

## Required Arduino libraries

Install these libraries in Arduino IDE:

- **M5Cardputer** by M5Stack
- **ArduinoJson** by Benoit Blanchon

ESP32 networking libraries such as `WiFi`, `HTTPClient` and `WiFiClientSecure` are provided by the ESP32 board package.

## Arduino IDE setup

1. Install Arduino IDE.
2. Add the M5Stack board package.
3. Install the **M5Stack** board package from Boards Manager.
4. Select **M5Cardputer** as the board.
5. Install the libraries listed above.
6. Open `firmware/CardputerAirMonitor.ino`.
7. For Demo Mode, keep the placeholder token.
8. For Live Mode, request your API token, insert it into the source code, and only then flash the firmware.
9. Select the Cardputer COM port.
10. Compile and upload.

More detailed instructions are in [docs/setup.md](docs/setup.md).

## Controls

### Home screen

| Key | Action |
|---|---|
| `M` | Open menu |
| `R` | Refresh live data / cycle Demo Mode scenario |
| `N` | Next monitored-area page |
| `P` | Previous monitored-area page |

### Main menu

The firmware includes:

- Wi-Fi setup
- Area management
- Volume settings
- Sound test
- Demo mode
- Manual API refresh
- System information
- Live debug
- Reboot

## Demo Mode

Demo Mode allows the UI, sounds and state transitions to be tested without relying on a real alert or API token.

Example scenarios include:

- All clear
- Kyiv: drones
- Kyiv: drones + ballistic threat
- Zhytomyr Raion: drones
- Both areas active
- MiG-31K scenario

See [examples/demo-mode.md](examples/demo-mode.md).

## Live Debug

The **LIVE DEBUG** menu is intended for development and troubleshooting.

It can display information such as:

- raw location UID
- oblast UID
- location type
- whether a record matched the configured area
- alert start time
- threat-item count
- decoded threat names

This is useful when the API schema or location matching needs to be checked.

## Project structure

```text
cardputer-air-alert-monitor/
├── README.md
├── LICENSE
├── CHANGELOG.md
├── .gitignore
├── firmware/
│   ├── CardputerAirMonitor.ino
│   └── README.md
├── docs/
│   ├── setup.md
│   ├── api.md
│   └── screenshots/
└── examples/
    └── demo-mode.md
```

## GitHub upload

See [docs/github-upload.md](docs/github-upload.md) for a simple browser-based upload guide.

## Security note

The current development firmware uses `WiFiClientSecure::setInsecure()` for HTTPS requests.

That is convenient during development, but it disables certificate verification. A future production-oriented version should validate the server certificate or CA chain.

## Disclaimer

This device is a hobby/community information display.

Do **not** rely on this project as your only source of emergency information. Always follow official emergency notifications and instructions.

API data may be delayed, unavailable, incomplete or changed by the upstream service.

## Roadmap

- [x] Wi-Fi setup on the Cardputer
- [x] Saved Wi-Fi credentials
- [x] Multiple monitored areas
- [x] Alert start time
- [x] Threat categories
- [x] Battery voltage
- [x] Demo mode without API token
- [x] API diagnostics
- [x] Live debug mode
- [ ] Better Ukrainian font support
- [ ] Region list loaded dynamically from API
- [ ] OTA firmware updates
- [ ] Certificate validation
- [ ] Cleaner configuration system
- [ ] Release builds

## Contributing

Issues and pull requests are welcome.

When reporting a bug, please include:

- Cardputer model
- Arduino IDE version
- M5Stack board package version
- M5Cardputer library version
- ArduinoJson version
- relevant API diagnostic values
- compiler error text, if applicable

Do not include your private API token.

## License

Released under the MIT License. See [LICENSE](LICENSE).
