# Uploading This Project to GitHub

## 1. Create a repository

On GitHub choose:

```text
+ → New repository
```

Recommended name:

```text
cardputer-air-alert-monitor
```

Recommended description:

```text
Real-time air alert monitor for M5Stack Cardputer using the alerts.in.ua API
```

Choose `Public` if you want the project to be open-source.

Because this project already contains a README, license and .gitignore, create the repository without generating extra starter files.

## 2. Check the API token

Before uploading, open:

```text
firmware/CardputerAirMonitor.ino
```

Make sure it contains:

```cpp
const char* API_TOKEN = "PASTE_YOUR_TOKEN_HERE";
```

Never upload your real API token.

## 3. Upload the project

In the empty GitHub repository choose:

```text
Add file → Upload files
```

Upload the contents of the `cardputer-air-alert-monitor` folder.

Use a commit message such as:

```text
Initial public release
```

Then click **Commit changes**.

## 4. Repository topics

Suggested topics:

```text
m5stack
cardputer
esp32
esp32-s3
arduino
embedded
iot
ukraine
alerts
alerts-in-ua
```

## 5. Add screenshots later

Put images in:

```text
docs/screenshots/
```

Then reference them from README.md, for example:

```md
![Main screen](docs/screenshots/home.jpg)
```
