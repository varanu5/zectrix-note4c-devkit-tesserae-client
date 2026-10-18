# ZecTrix NOTE4C Devkit · Tesserae firmware

> [!WARNING]
> **For the ZecTrix NOTE4C Devkit with the four-colour display only! Do not flash this firmware onto the grayscale NOTE4 variant.**

Display pictures from your own [Tesserae](https://github.com/dmellok/tesserae) server on the **ZecTrix NOTE4C Devkit**. Configure Wi-Fi and your server from a phone, then let the device check for pictures on its schedule.

**Version 1.1.1 · ESP-IDF 6.1.** REST and Cloud relay support. Display operation, fast refresh, charging indication and normal sleep/wake cycles have been tested on hardware; broader recovery testing and battery characterization are still in progress. See [CHANGELOG.md](CHANGELOG.md).

## What it does

- Downloads pictures over Wi-Fi and skips repainting when the image is unchanged.
- Sleeps between scheduled checks, with buttons for an immediate check or server action. After three failed connection attempts on separate wakes, it waits at least one hour before retrying; longer saved intervals are preserved.
- Provides a phone-friendly setup page and built-in setup, registration and low-battery screens.
- Offers Native, 5-second and 10-second refresh choices. Native is the default; faster modes can produce less saturated colours.
- Reports battery level, signal strength and status. REST also supports requested diagnostic log uploads.
- Shows charging status on the LED, including brief charger-event wakes while sleeping.

## Before you start

- **ZecTrix NOTE4C Devkit**, ESP32-S3 with 16 MB flash, 8 MB PSRAM and the 400 × 300 four-colour panel. This firmware is not for the grayscale NOTE4.
- A USB-C **data** cable and a computer for flashing.
- A password-protected 2.4 GHz Wi-Fi network and a reachable Tesserae server with Note4c device support installed.
- **A verified backup is strongly recommended before the first installation, but is optional.** Keep it private: it can contain credentials and device-specific settings.

## Install and set up

1. **Back up your device (strongly recommended, optional).** A verified backup gives you a way to restore the original firmware. If you choose to skip it, continue to step 2. To make a backup, install Python and esptool 5.x using the [tool setup instructions](FLASHING.md#1-prepare-the-tools-and-connect). Connect a USB-C data cable, close serial monitors and open a new, empty backup folder in your terminal. List the available ports:

   ```sh
   python -m serial.tools.list_ports
   ```

   Replace `PORT` below with your device's port, such as `/dev/cu.usbmodem1101` on macOS, `/dev/ttyACM0` on Linux or `COM3` on Windows. Read the complete 16 MB flash:

   ```sh
   python -m esptool --chip esp32s3 --port PORT --baud 460800 --after no-reset read-flash 0x0 0x1000000 original-firmware.bin
   ```

   After the read finishes, verify the backup against the device:

   ```sh
   python -m esptool --chip esp32s3 --port PORT --baud 460800 --after no-reset verify-flash 0x0 original-firmware.bin
   ```

   Successful verification confirms that the backup matches the device. Keep the verified backup somewhere safe and private; it may contain Wi-Fi credentials and device-specific settings. Do not overwrite an earlier backup. The message `Staying in bootloader` is expected and leaves the device ready for flashing. If the port disappears or cannot connect, follow the [connection troubleshooting steps](FLASHING.md#connection-troubleshooting).

2. **Open the [web installer](https://note4ctesserae.pages.dev) and flash.** Use desktop Chrome or Edge, click **Connect device** and select your Note4c. If your device does not appear, try holding the **Front button** until the USB device appears, then release it and select the device. Choose **Fresh install** for the first installation, or **Update** to keep settings on a compatible installation. Click **Install firmware** and keep USB connected until verification completes. The website does not create a backup automatically. See the [flashing guide](FLASHING.md) for more details.
3. **For a fresh installation, join the setup network** shown on the display: `Note4c-Setup-XXXX`, password `tesserae`. If setup does not open automatically, visit **http://192.168.4.1**.
4. **Enter Wi-Fi details**, choose REST or Cloud relay, complete its connection details and choose a device name and refresh mode, then press **Save & restart**. For an existing network, leaving its password blank keeps the saved password.
5. **For REST**, register the device in Tesserae under **Settings → Devices**, or use a pairing code during setup. **For Cloud relay**, configure relay on your server and create a pairing code there before entering it in device setup. Use **ZecTrix NOTE4C Devkit** and **Nominal BWRY**. Send an image or assign a lineup and schedule.

Cloud relay receives encrypted images and settings, with the same low-battery protection and outage retry policy as REST. If removal is confirmed, the display shows **Unpaired** and sleeps for 24 hours. Front wakes it; hold Front for 20 seconds to reconfigure. It never automatically switches to REST. Leaving the relay pairing code blank when saving unchanged relay details keeps the existing pairing.

Relay navigation and refresh actions wait up to 45 seconds, checking every five seconds and stopping early when a new image arrives. Normal scheduled checks have no additional wait. MQTT, Bluetooth and Wi-Fi firmware updates are not implemented yet.

## Buttons and LED

| Control | Action |
| --- | --- |
| Front: short press | Wake and check for a new image; unchanged images are not repainted |
| Front: hold 5–10 seconds, then release during the steady LED cue | Ask the server for a fresh render and repaint its response |
| Front: hold 20 seconds, then release at the three-flash cue | Open setup |
| Lower side button | Wake and send the server's `right` action |
| Upper side button | Unused |

Front and lower-button actions receive a brief LED acknowledgement; scheduled checks do not. The LED stays on while charging and turns off when charging stops, according to the charger hardware rather than the estimated battery percentage.

## Known limitations

- Battery percentage is an estimate; battery life and sleep current have not been characterized.
- Watchdog resets reported with USB serial monitoring remain under investigation.
- Fast-refresh colour quality varies. Use Native when colour quality matters most.

## License and credits

Created by **[varanu5](https://github.com/varanu5)**  as a hobby project for my own needs, and shared with the community in the hope that others find it useful.

This is independent community firmware for the ZECTRIX NOTE4C Devkit, and is not affiliated with or endorsed by ZECTRIX. Please use the firmware at your own risk. I cannot take responsibility for damaged or bricked devices, or lost data.

Thanks to the Tesserae project and the firmware, display, font, QR-library etc. [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Licensed under **AGPL-3.0-or-later**; see [LICENSE](LICENSE). Third-party components retain their own licenses and notices.
