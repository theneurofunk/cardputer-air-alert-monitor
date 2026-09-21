# Setup Guide

This guide explains how to compile and upload the firmware to an original M5Stack Cardputer.

## 1. Install Arduino IDE

Install the current Arduino IDE 2.x release.

## 2. Add the M5Stack board package

Open:

**File → Preferences**

Add the M5Stack package index URL to **Additional Boards Manager URLs**:

```text
https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json
```

Then open:

**Tools → Board → Boards Manager**

Search for:

```text
M5Stack
```

Install **M5Stack by M5Stack**.

## 3. Select the board

Open:

**Tools → Board**

Select:

```text
M5Cardputer
```

This project targets the original Cardputer based on StampS3 / ESP32-S3.

## 4. Install libraries

Open:

**Sketch → Include Library → Manage Libraries**

Install:

```text
M5Cardputer
ArduinoJson
```

## 5. Choose how you want to use the firmware

There are two ways to use the project.

### Demo Mode

Demo Mode does **not** require an API token.

You can flash the firmware with:

```cpp
const char* API_TOKEN = "PASTE_YOUR_TOKEN_HERE";
```

Then enable:

```text
M → DEMO MODE
```

Demo Mode uses simulated alerts and is useful for testing:

- the display
- menus
- sounds
- alert states
- threat labels
- alert start times

### Live Mode

Live Mode uses real data from alerts.in.ua and requires your own personal API token.

The token must be inserted into the source code **before you flash the firmware**.

It is not entered through the Cardputer keyboard after installation.

## 6. Request an alerts.in.ua API token for Live Mode

Open:

```text
https://devs.alerts.in.ua/
```

Find the API access / token request section and submit a request to the developers.

Example project information:

**Project name**

```text
M5Stack Cardputer Air Alert Monitor
```

**Description**

```text
A personal non-commercial IoT device based on the M5Stack Cardputer.
The device displays air raid alert status, alert start time and available
threat information for selected Ukrainian regions using the alerts.in.ua API.
```

**Usage**

```text
Personal / non-commercial use.
```

Wait for the developers to provide your personal API token.

## 7. Insert the token before flashing

Open:

```text
firmware/CardputerAirMonitor.ino
```

Find:

```cpp
const char* API_TOKEN = "PASTE_YOUR_TOKEN_HERE";
```

Replace the placeholder:

```cpp
const char* API_TOKEN = "your_personal_api_token";
```

Now compile and flash the firmware.

If the token is changed later, update the line and flash the firmware again.

### Important security note

Never upload your real token to a public GitHub repository.

Before publishing your source code, restore:

```cpp
const char* API_TOKEN = "PASTE_YOUR_TOKEN_HERE";
```

Each user should request their own token.

## 8. Upload

Connect the Cardputer using a USB data cable.

Choose the correct COM/serial port in Arduino IDE and click **Upload**.

If the board does not enter download mode automatically, use the Cardputer download/boot procedure and reconnect the USB port.

## 9. Configure Wi-Fi

For Live Mode, on the Cardputer:

1. Press `M`
2. Open **WiFi**
3. Scan for networks
4. Select your network
5. Enter the Wi-Fi password
6. Wait for `CONNECTED`

The credentials are saved in ESP32 Preferences.

## 10. Verify API communication

Open:

```text
M → System info
```

A healthy live connection should normally show HTTP `200` for the live endpoints and `JSON: OK`.

## Troubleshooting

### Demo Mode works, but Live Mode does not

Check:

- API token was inserted before flashing
- Wi-Fi connection is active
- token is valid
- Demo Mode is disabled

### API status is negative

A negative HTTPClient status usually indicates a connection/transport problem instead of a normal HTTP status code.

Check:

- Wi-Fi connection
- DNS/internet access
- API host availability
- TLS/connection state

### HTTP 401 / 403

Check the API token.

### HTTP 429

The API rate limit has been reached. Wait before retrying.

### JSON errors

Open **System info** and **LIVE DEBUG** and record:

- HTTP status
- payload size
- JSON status
- alert count
- air-alert count
- match count

Do not publish your API token in an issue.
