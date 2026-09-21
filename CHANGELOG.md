# Changelog

All notable changes to this project will be documented here.

## [Unreleased]

### Added
- Clear Demo Mode / Live Mode documentation
- Demo Mode works without an API token
- API token request instructions
- Explicit note that the API token must be inserted into the source code before flashing
- Live API debug mode
- Multiple monitored areas
- Wi-Fi scanner and saved credentials
- Battery percentage and voltage
- Distinct alert sounds
- Threat-category display
- Alert start-time display
- API/system diagnostics

### Development notes
- Default monitored areas are Kyiv City and Zhytomyr Raion.
- Real-time data is retrieved from alerts.in.ua in Live Mode.
- HTTPS currently uses `setInsecure()` during development.

## [0.1.0] - 2026-09-21

### Added
- Initial public project structure.
