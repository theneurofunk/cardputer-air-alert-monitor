# API Notes

The firmware uses the alerts.in.ua API for Live Mode.

Demo Mode does not call the live API and does not require an API token.

## API token

For Live Mode, every user should request their own personal API token from:

```text
https://devs.alerts.in.ua/
```

The token must be inserted into the firmware source code **before flashing** the Cardputer:

```cpp
const char* API_TOKEN = "your_personal_api_token";
```

Do not publish the real token in a public repository.

## Live status endpoint

The compact endpoint is used for fast state polling:

```text
/v1/iot/active_air_raid_alerts.json
```

The firmware reads the state associated with the configured location UID.

Recognized state characters:

```text
A = active alert
P = partial alert
N = no alert
```

## Active-alert details

The detailed endpoint is used for metadata:

```text
/v1/alerts/active.json
```

The firmware currently looks for fields such as:

```text
alert_type
location_uid
location_oblast_uid
location_title
location_title_en
location_type
location_raion
started_at
threats
threat_type
```

Only `air_raid` records are used for the main air-alert display.

## Default locations

The project currently ships with:

```text
Kyiv City      UID 31
Zhytomyr Raion UID 59
```

The firmware also contains matching logic for region-wide records and raion-level records.

## Threat types

Known values handled by the firmware include:

```text
tactic_aircraft_activity
strategic_aircraft_activity
mig31k_departure
ballistic_missiles
cruise_missiles
unspecified_missiles
drones
guided_aerial_bombs
air_defense
```

Unknown values are displayed as `UNKNOWN`.

## Polling

The default polling interval is:

```text
20 seconds
```

Keep API rate limits in mind if you change this.

## HTTPS

The development build currently uses:

```cpp
client.setInsecure();
```

This disables certificate validation.

It is convenient for development/testing, but certificate validation is recommended for a production-oriented release.

## Important

The API is an upstream dependency. Endpoint formats, fields and limits may change.

If matching stops working, use the firmware's **LIVE DEBUG** mode before changing location rules.
