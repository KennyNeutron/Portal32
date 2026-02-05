# Portal32

Lightweight ESP32 configuration & provisioning portal with HTML, CSS, and JavaScript support for Wi-Fi onboarding and device setup.

## Features

- **Responsive Setup Portal**: Modern UI for WiFi scanning and credential entry.
- **Captive Portal**: DNS redirection for automatic setup page loading.
- **Persistence**: Remembers settings across reboots using NVS (Preferences).
- **Auto-Recovery**: Automatically falls back to Access Point mode if the configured network is unavailable.
- **MDNS Support**: Access your device via `[device-name].local`.

## Getting Started

1. **Flash**: Upload `Portal32.ino` to your ESP32.
2. **Access Point**: Look for a WiFi network named **Portal32** on your mobile device.
3. **Configure**: Once connected, the setup page (192.168.4.1) will open automatically.
4. **Onboard**: Select your network, enter the password, and name your device.
5. **Reboot**: The device will save settings and reboot to connect to your station network.

## API Endpoints

- `GET /`: Serves the captive portal UI.
- `GET /scan`: Returns a JSON list of available WiFi networks.
- `POST /save`: Saves credentials and triggers a reboot.
- `GET /status`: Returns the current connection state.

## Technical Requirements

- ESP32 Board
- Arduino IDE or PlatformIO
- Libraries: `WiFi`, `WebServer`, `DNSServer`, `Preferences`, `ESPmDNS`
