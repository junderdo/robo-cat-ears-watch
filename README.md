# Robo Cat Ears Watch

Firmware for the Robo Cat Ears wrist controller: a
[Waveshare ESP32-S3 Touch AMOLED 2.06](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.06)
running ESP-IDF, with an LVGL UI built on
[esp-brookesia](https://github.com/espressif/esp-brookesia). The watch connects to the ears over
Bluetooth LE and plays their animations, sets their lighting and mode, and calibrates their servos.

It is one of three parts of the product:

- [`robo-cat-ears`](https://github.com/junderdo/robo-cat-ears): the ears firmware, a BLE GATT
  server that owns the animation store and the wire protocol.
- **`robo-cat-ears-watch`** (this repo): the wrist controller, a BLE client that plays animations
  but doesn't store them.
- [`milk-lab-creations`](https://github.com/junderdo/milk-lab-creations): the web app where
  animations are authored and saved to the ears.

## Build and flash

Requires ESP-IDF 5.5.2. The target (`esp32s3`) comes from the committed `sdkconfig`, so there's no
`set-target` step.

```bash
idf.py build
idf.py -p /dev/ttyACM0 flash monitor     # Ctrl-] exits the monitor
idf.py menuconfig
```

The board's USB-C port is the ESP32-S3's native USB (Espressif `303a:1001`), which shows up as
`/dev/ttyACM0` on Linux. On WSL it needs forwarding first, as described in the next section.

`esp_lvgl_port` is pinned `<2.8.0` in `main/idf_component.yml`. 2.8+ uses
`LV_COLOR_FORMAT_RGB565_SWAPPED`, which the BSP's LVGL 9.2.2 lacks, so leave the pin in place even
if the component manager suggests an upgrade.

## How to set up connection to the watch in WSL

If you run ESP-IDF in Windows Subsystem for Linux (WSL), the watch's USB port has to be forwarded
from Windows before you can flash or monitor it.

Prerequisites:

- WSL 2, with ESP-IDF installed and configured inside it.
- `lsusb` in WSL:
  ```bash
  sudo apt-get install usbutils
  ```
- [usbipd-win](https://github.com/dorssel/usbipd-win/releases) installed on Windows.

Steps:

1. Open PowerShell as Administrator and list the USB devices:
   ```powershell
   usbipd list
   ```
2. Find the watch in the list (`USB JTAG/serial debug unit`, `303a:1001`) and note its BUSID.
3. Share the device. This is needed once per device:
   ```powershell
   usbipd bind --busid <BUSID>
   ```
4. Attach it to WSL. This is needed after every replug or reboot:
   ```powershell
   usbipd attach --wsl --busid <BUSID>
   ```
5. In WSL, check that it arrived:
   ```bash
   lsusb
   ```
   You should see something like `Bus 001 Device 002: ID 303a:1001 Espressif USB JTAG/serial debug unit`.
6. Add your user to the `dialout` group so you can open the serial port, then log out and back in:
   ```bash
   sudo usermod -aG dialout $USER
   ```
   To get access without logging out, `sudo chmod 0666 /dev/ttyACM0` works until the device is
   next replugged.
7. Flash and monitor. The rest of this README assumes `/dev/ttyACM0`, but yours may differ:
   ```bash
   idf.py -p /dev/ttyACM0 flash monitor
   ```
   In VS Code, set the same port in the ESP-IDF extension's status bar and use its buttons instead.
8. When you're done, give the device back to Windows from PowerShell:
   ```powershell
   usbipd detach --busid <BUSID>
   ```

## Project layout

```
main/                  boot and init (main.cpp), the dark stylesheet (dark/), system status (system/)
components/
  brookesia_app_robo_cat_ears/   the Robo Cat Ears app: one .cpp/.hpp pair per screen in screens/
  brookesia_app_system_info/     a system info app showing the AXP2101 PMU's readings
  services/                      one component per domain service (see below)
  brookesia_core/, XPowersLib/   vendored third-party code
tools/                 image conversion helper
```

The Robo Cat Ears app's screens are **scan** (find and connect to the ears), **animate**,
**modes**, **glow** and **pick color** (lighting), **calibration** and **settings**. Screens talk
only to services, and the services own all the BLE traffic:

| Service | Owns |
| --- | --- |
| `bluetooth_service` | Scanning for the ears, the GATT client connection, sending commands and receiving replies |
| `animation_store_service` | Reading the ears' animation store and playing slots by index |
| `animation_mode_service` | The ears' animation mode |
| `lighting_service` | The ears' LED lighting |
| `calibration_service` | Servo calibration |
| `power_service` | The watch's power ladder (auto-dim, then panel off) and screen brightness |

`components/services/` is one level deeper than ESP-IDF scans, so the root `CMakeLists.txt` lists
it in `EXTRA_COMPONENT_DIRS`. A new service directory there is picked up automatically.

## Bluetooth LE communications

### Protocols

The watch uses [GAP](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/bluetooth/esp_gap_ble.html)
to discover the ears and the
[GATT client API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/bluetooth/esp_gattc.html)
to exchange data with them. It is a **GATT client**, and the ears are the server.

### The wire contract

The protocol is specified in
[`docs/ble-protocol.md` in `robo-cat-ears`](https://github.com/junderdo/robo-cat-ears/blob/main/docs/ble-protocol.md),
which owns it. Change it there first, then update this repo to match. `DataType` in
`bluetooth_service.hpp` mirrors the protocol's type bytes:

| Type byte | Data |
| --- | --- |
| `0x01` | Animation |
| `0x02` | Lighting |
| `0x03` | Calibration |
| `0x04` | Animation mode |
| `0x06` | Animation store |

Three rules this watch follows:

- **The watch owns no animations.** It reads the ears' store when it connects, caches it for the
  life of that connection, and plays animations by slot index. Creating, renaming, deleting and
  saving animations belong to the web app.
- **The store protocol version must match exactly.** If the ears report a
  `ANIMATION_STORE_PROTOCOL_VERSION` the watch doesn't know, it disconnects instead of guessing.
- **Reply length checks are minimums.** For example, `CAPABILITY` is checked with `< 4`, not
  `!= 4`, so the ears can append fields (they now append a six-byte device serial) without
  breaking older watches.

## Adding images and graphics

Images are compiled in as C arrays in `LV_COLOR_FORMAT_ARGB8888` and live under an app's
`assets/` directory. To add one:

1. Export the image from Img2Lcd as a 24-bit RGB C array.
2. Convert it to 32-bit RGBA with `tools/convert_rgb24_to_rgba32.py`. The script adds an alpha
   channel and rounds the corners (radius 20 by default; set it with `-r`):
   ```bash
   python3 tools/convert_rgb24_to_rgba32.py input_rgb24.c output_rgba32.c -r 20
   ```
3. Check the output into the app's `assets/` and reference it from an `lv_image_dsc_t` with
   `LV_COLOR_FORMAT_ARGB8888`.

## Testing

This repo has no host tests, and nothing in it builds off-target. To verify a change, build it,
then flash it and try it on the watch against a set of ears.

## Issue tracking

Issues are cards on the [Robo Cat Ears Trello board](https://trello.com/b/DHDPlEuL/robo-cat-ears),
which covers the whole product. See [`docs/agents/issue-tracker.md`](docs/agents/issue-tracker.md).

## License

GPL-3.0-or-later. See [`LICENSE`](LICENSE).
