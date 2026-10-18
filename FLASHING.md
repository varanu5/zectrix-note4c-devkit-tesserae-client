# Install or update firmware

For the **ZecTrix NOTE4C Devkit** with 16 MB flash. Choose [web flashing](#flash-using-the-website) or [manual flashing](#flash-manually). You only need one method.

## Flash using the website

Before your first installation, **back up and verify the device** using the [backup steps below](#2-back-up-and-verify-before-flashing) or the website’s backup guide. The website does not create a backup automatically.

1. Open **[note4ctesserae.pages.dev](https://note4ctesserae.pages.dev)** in desktop **Chrome or Edge**.
2. Connect a USB-C data cable and close other serial tools. Click **Connect device** and select the USB JTAG/serial device. No button press is needed if it already appears. If it is missing, briefly hold the front button and release it when USB appears, then try again.
3. Choose **Fresh install** for a new installation or a settings reset. Choose **Update** to retain Wi-Fi, server settings and pairing on a recognized compatible installation.
4. Click **Install firmware** and keep USB connected until verification completes.
5. Keep the front button released and wait for the installer to request an automatic restart. If the device does not start, reconnect the USB cable or press **RESET**. For a fresh install, join the setup Wi-Fi shown on the screen and open **http://192.168.4.1** to enter your Wi-Fi/server details. An update uses your saved settings.

If Update is unavailable, do not choose Fresh install just to bypass it when you need to keep settings.

The website also has a **Serial monitor** for viewing device logs. You can stop here; the commands below are the alternative manual method.

## Flash manually

Follow these steps in order. Make and verify the backup before writing firmware.

### 1. Prepare the tools and connect

Use a USB-C **data** cable and close any serial monitors or other flashing tools. No button press is needed if the serial port already appears. If it is missing, briefly hold the front circular button and release it when USB appears.

Backup and manual flashing use **esptool 5.x**. If ESP-IDF 6.1 is already installed, activate its environment. Otherwise, install Python and set up the tools:

**macOS / Linux:**

```sh
python3 -m venv "$HOME/note4c-tools"
. "$HOME/note4c-tools/bin/activate"
python -m pip install "esptool>=5,<6" esp-idf-monitor
```

**Windows PowerShell:**

```powershell
py -m venv "$HOME\note4c-tools"
& "$HOME\note4c-tools\Scripts\Activate.ps1"
python -m pip install "esptool>=5,<6" esp-idf-monitor
```

Tool installation needs internet access. Once installed, backup and manual flashing work offline with downloaded firmware files. Keep this terminal open for the following commands.

List the serial ports:

```sh
python -m serial.tools.list_ports
```

**Replace `PORT` in every command** with your device's port, for example `/dev/cu.usbmodem1101` on macOS, `/dev/ttyACM0` on Linux or `COM3` on Windows. Confirm the chip and flash size:

```sh
python -m esptool --chip esp32s3 --port PORT --after no-reset flash-id
```

The output should identify **ESP32-S3** and **16 MB flash**.

### 2. Back up and verify before flashing

Create a **new empty folder** for this device's backup and open it in your terminal. Replace the example path:

```sh
cd "PATH_TO_NEW_BACKUP_FOLDER"
python -m esptool --chip esp32s3 --port PORT --baud 460800 --after no-reset read-flash 0x0 0x1000000 original-firmware.bin
```

Wait for the full **16,777,216 bytes** to finish, then verify the saved file against the device:

```sh
python -m esptool --chip esp32s3 --port PORT --baud 460800 --after no-reset verify-flash 0x0 original-firmware.bin
```

**Continue only after verification succeeds.** Keep this backup in a safe place, separate from firmware downloads. Never overwrite an earlier backup or publish it: it can contain Wi-Fi credentials and device-specific settings. Use a separate backup folder for each device and each backup.

`Staying in bootloader` is expected here because the commands use `--after no-reset`. It leaves the device ready for installation. The website does not create a backup automatically.

### 3. Download the release and choose an install type

Download the firmware files from one [GitHub Release](https://github.com/varanu5/zectrix-note4c-devkit-tesserae-client/releases). Keep files from the same release together, then open that folder:

```sh
cd "PATH_TO_DOWNLOADED_FIRMWARE"
```

If you downloaded all release assets, verify `SHA256SUMS` using `shasum -a 256 -c SHA256SUMS` on macOS or `sha256sum -c SHA256SUMS` on Linux. On Windows, use `Get-FileHash FILENAME -Algorithm SHA256` and compare each selected file with its entry in `SHA256SUMS`.

**Choose only one of the following paths. Do not run a fresh-install command when keeping settings.**

#### Fresh install: start over

This installs the firmware and clears Wi-Fi, server settings and pairing. After completing the backup above, use the single merged image:

```sh
python -m esptool --chip esp32s3 --port PORT --baud 460800 --after hard-reset write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0 zectrix-note4c-devkit-tesserae-client-fresh-install.bin
```

Alternatively, if you downloaded the separate images, use `flash_args_fresh` with all six files it names in the same folder:

```sh
python -m esptool --chip esp32s3 --port PORT --baud 460800 --after hard-reset write-flash "@flash_args_fresh"
```

Use either command, not both. Wait for successful completion before continuing to step 4.

#### Update: keep existing settings

Use this only for an existing compatible Note4c installation using this project's `note4c-ab-4m-v1` layout. It is not a settings-preserving migration from the manufacturer's or another project's firmware. If unsure, let the web installer check compatibility.

Place these files together: `bootloader.bin`, `partition-table.bin`, `ota_data_initial.bin`, `ota_1_blank.bin`, `zectrix-note4c-devkit-tesserae-client.bin` and `flash_args`.

```sh
python -m esptool --chip esp32s3 --port PORT --baud 460800 --after hard-reset write-flash "@flash_args"
```

This preserves saved settings and invalidates the old inactive application. Do not add `--erase-all`, `nvs_blank.bin`, or use the fresh-install image. Wait for successful completion before continuing.

### 4. Start the device and view logs

The manual flashing commands above request an automatic restart, as does the updated web installer after verification. If the device stays in download mode, release the front button and reconnect the USB cable or press **RESET**.

- **Fresh install:** join the displayed `Note4c-Setup-XXXX` Wi-Fi network (password `tesserae`). Open **http://192.168.4.1**, enter Wi-Fi and server details, then press **Save & restart**. Choose REST and register the device when prompted, or choose Cloud relay and enter a fresh pairing code from your server.
- **Update:** the device reconnects using its saved settings.

Use the website's **Serial monitor**, or run this in the terminal after closing other serial tools:

```sh
python -m esp_idf_monitor --port PORT --baud 115200 --target esp32s3 --no-reset
```

Exit with **Ctrl+]**. USB disconnects during deep sleep are normal; the monitor waits for reconnection. Recheck the port if it changes. A locally built matching `.elf` file can be supplied for decoded crash traces.

## Restore your backup

Use the verified full-flash backup from **this same device**. Restoration replaces the firmware and current saved settings with the backed-up contents. Open your backup folder and run:

```sh
python -m esptool --chip esp32s3 --port PORT --baud 460800 --after no-reset write-flash 0x0 original-firmware.bin
python -m esptool --chip esp32s3 --port PORT --baud 460800 --after no-reset verify-flash 0x0 original-firmware.bin
```

Run verification only after the write succeeds. These restore commands leave the device in the bootloader. After verification succeeds, release the front button and reconnect the USB cable or press **RESET**.

## Connection troubleshooting

If the serial port already appears, connect directly without pressing any buttons. Otherwise, try the front-button wake method and a different USB data cable. Close other monitors and browser serial connections. If necessary, keep USB connected, hold the front button, press and release the recessed RESET on the left side, then release the front button and reconnect. This forces download mode.