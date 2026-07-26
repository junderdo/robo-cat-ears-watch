If you are using Windows Subsystem for Linux (WSL) to run ESP-IDF, you may encounter issues with serial port access when trying to flash or monitor your ESP32 device. Follow these steps to set up serial port access in WSL:

Preqrequisites:
- Ensure you have WSL 2 installed and set up on your Windows machine.
- Ensure you have ESP-IDF installed and configured in your WSL environment.
- Ensure you have lsusb installed in your WSL environment. You can install it using the following command:
   ```bash
   sudo apt-get install usbutils
   ```
- Install USB drivers https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers
   - The installer is named something like (CP210x Windows Drivers)
- Install usb-pid in PowerShell: https://github.com/dorssel/usbipd-win/releases

Steps:
1. Open PowerShell as Administrator and run the following command to list available USB devices:
   ```powershell
   usbipd list
   ```
2. Identify your ESP32 device from the list and note its BUSID
3. Make the ESP32 device sharable using the following command, replacing <BUSID> with the actual BUSID of your device:
   ```powershell
   usbipd bind --busid <BUSID>
   ```
4. Attach the ESP32 device to WSL using the following command, replacing <BUSID> with the actual BUSID of your device:
   ```powershell
   usbipd attach --wsl --busid <BUSID>
   ```
5. Open your WSL terminal and verify that the device is accessible by listing the serial devices:
   ```bash
   lsusb
   ```
6. You should see your ESP32 device listed (e.g., Bus 001 Device 002: ID 303a:1001 Espressif USB JTAG/serial debug unit). You can now use this device path in your ESP-IDF commands to flash and monitor your ESP32 device.
7. Set your ESP-IDF serial port at the bottom of your VSCode window eg. /dev/ttyACM0 -- The rest of this guide will use /dev/ttyACM0 as the serial port it may be different on your system
8. Make sure your serial port permissions are set correctly. Replace the example port with your actual port if different:
   ```bash
   sudo chmod 0666 /dev/ttyACM0
   ```
9. Make sure you have added your user to the dialout group to avoid permission issues:
   ```bash
   sudo usermod -aG dialout $USER
   ```
   After running this command, you may need to log out and log back in for the changes to take effect.
10. Make sure you are using the UART connection for flashing and monitoring in ESP-IDF. You can specify the port using the `-p` option in your ESP-IDF commands.
11. You can now flash and monitor your ESP32 device using ESP-IDF commands in WSL. For example:
   ```bash
   idf.py -p /dev/ttyACM0 flash monitor
   ```
   Or by using the VSCode extention utils on the bottom bar
12. To unbind the device from WSL when you're done, run the following command in PowerShell, replacing <BUSID> with the actual BUSID of your device:
   ```powershell
   usbipd wsl unbind --busid <BUSID>


# Adding images and graphics

Use Img2Lcd to convert to 32 bit color (LV_COLOR_FORMAT_ARGB8888) -- TODO: figure out what ordering the bytes are in