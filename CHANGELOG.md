# Changelog

## 1.1.1

- Show the Cloud relay address as a grey example when no address is saved; require an entered address for setup.

- Extend Cloud relay navigation/refresh response waiting to 45 seconds, polling every five seconds and stopping early when a new image arrives.
- Keep short Front presses, scheduled checks, REST behavior and saved settings unchanged.

## 1.1.0

- Cloud relay setup, encrypted image/config reception and battery/status reporting.
- Unpaired screen after confirmed relay removal; 24-hour sleep and manual re-setup, with no automatic switch to REST.
- Shared low-battery protection and outage retry policy for both transports.
- Existing REST settings migrate without losing Wi-Fi, pairing or display preferences.

## 1.0.0

First public release of **ZecTrix NOTE4C Devkit** firmware for Tesserae, built with **ESP-IDF 6.1**.

- Wi-Fi setup from a phone, pairing, HTTP/HTTPS image checks and server-controlled scheduling.
- Native, 5s and 10s refresh for images and built-in screens. Native is the default; fast modes trade colour saturation for speed.
- Deep sleep between checks, radio off during painting, panel shutdown and a one-hour minimum retry after three consecutive connection failures. Longer saved intervals are preserved.
- Front-button image checks, fresh-render hold and setup hold; lower-button server actions.
- Charging LED, low-battery screen, battery/status reports and 6 KiB retained diagnostic logs.
- Password-protected Wi-Fi, saved-setting retention and recovery after failed setup.
- Fresh-install and settings-preserving update binaries, with checksums and backup instructions.
- Bundled cJSON for offline builds after installing the SDK and toolchain.