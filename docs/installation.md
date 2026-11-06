---
title: Installation
nav_order: 2
---

# Installation

## Supported Devices

- Xteink X3, X4
- Xteink X4 Classic and X4 Pro
- Seeed Studio Sticky

Don't have a device yet? Get one directly from [Xteink](https://go.sjv.io/X4RGBb) or [Seeed Studio](https://www.seeedstudio.com/reTerminal-Sticky-p-6861.html?sensecap_affiliate=1Nxo3Gw&referring_service=link).

Note: Your purchases using the above affiliate links help support ongoing development of Crossink.

## Which File Do I Need?

CrossRose has two kinds of releases:

- **Stable** ([latest stable release](https://github.com/crossrose-reader/CrossRose/releases/latest)): tested releases, numbered like `v0.1.0`. Version 1.0.0 will be the first big release. Pick this one if you are not sure.
- **Nightly** ([nightly release](https://github.com/crossrose-reader/CrossRose/releases/tag/nightly)): the newest code, built every night. For testing. It can have bugs.

Each release has one `firmware-*.bin` file per device. Download the one for your device:

| Device | File |
| --- | --- |
| Xteink X3, X4 | `firmware-x3-x4-*.bin` |
| Xteink X4 Pro | `firmware-x4-pro-*.bin` |
| Xteink X4 Classic | `firmware-x4-classic-*.bin` |
| Seeed Studio Sticky | `firmware-sticky-*.bin` |

To go back to an earlier version, download that version's file from the [releases page](https://github.com/crossrose-reader/CrossRose/releases) and install it the same way.

## Web Installation via USB

#### For new installs and updates.

Inky, CrossInk's web app, can only install CrossInk. To install CrossRose over
USB, use the CrossPoint web installer. CrossPoint is the project that CrossInk
and CrossRose are built on. Its installer can flash a `.bin` file from your
computer.

1. Download the `firmware-*.bin` file for your device (see [Which File Do I Need?](#which-file-do-i-need)).
2. Open [https://crosspointreader.com/install](https://crosspointreader.com/install) in Chrome or Edge on a computer.
3. Select your device. The X4 Classic is listed as **Xteink X4C**.
4. Open **Advanced options**, click **Custom .bin**, and choose the file you downloaded.
5. Connect the reader with a USB-C data cable. Click **Flash**, choose your reader in the browser's list, and click **Connect**.
6. Keep the cable connected until the page says the firmware is installed. Then follow the restart steps on the page.

If the reader does not show up in the browser's list, try another cable or USB
port. If it still does not show up, USB may be turned off on your device: see
[USB Locked Devices](#usb-locked-devices).

## USB Drive

On X4 Pro, choose `Home > File Transfer > USB Drive` to expose the SD card to
your computer. Eject the drive from the computer before disconnecting it; the
reader restarts to Home when the drive is safely ejected or the cable is
removed.

## SD Card Firmware Update

#### For updating CrossRose, or switching from CrossInk to CrossRose. Can be used by USB locked devices.

Use this when your reader already runs CrossRose, CrossInk, or another firmware
with an SD card update menu.

1. Download the `firmware-*.bin` file for your device (see [Which File Do I Need?](#which-file-do-i-need)).
2. Place the file on your SD card. You can place this file anywhere.
3. Go to `Settings > System > SD Card Firmware Update` and navigate to the `.bin` file and update.

## USB Locked Devices

Some readers have USB data transfer disabled. The web installer cannot reach
them, so use the SD card instead.

**If the reader already runs CrossInk, CrossPoint, or another firmware with an
SD card update menu:** follow [SD Card Firmware Update](#sd-card-firmware-update).

**If the reader still runs the stock Xteink firmware:** install CrossInk first,
then switch to CrossRose.

1. Go to [Inky](https://inky.crossink.dev/#flash-tools), check "I have a locked device", select your device, and follow the steps to install CrossInk. On the X4 Pro and X4 Classic, Inky first sends you to the [OTA Unlocker](https://crosspointreader.com/unlocker).
2. Follow [SD Card Firmware Update](#sd-card-firmware-update) to install CrossRose.

CrossInk is marked "safe" on [Brick Club](https://brickclub.pages.dev/), a
community list of firmware for locked devices. CrossRose is not on that list
yet, so this guide takes the two-step way.

## Command Line

These instructions are for macOS and Linux. Windows users should use the web installer.

Install `esptool`:

```sh
pip3 install esptool
```

Download the `firmware-*.bin` file from the [releases page](https://github.com/crossrose-reader/CrossRose/releases), then connect your device with USB-C.

Find the device port:

```sh
# Linux
dmesg | grep tty

# macOS
ls /dev/cu.*
```

Flash the firmware:

```sh
# Linux
esptool.py --chip esp32c3 --port /dev/ttyACM0 --baud 921600 write_flash 0x10000 /path/to/firmware.bin

# macOS
esptool.py --chip esp32c3 --port /dev/cu.usbmodem2101 --baud 921600 write_flash 0x10000 /path/to/firmware.bin
```

Replace the port and firmware path with your actual values.

The X4 Pro, X4 Classic and Sticky use an ESP32-S3 chip: write `--chip esp32s3`
instead of `--chip esp32c3`.

The reader keeps firmware in two slots. This command always writes to the first
one (`0x10000`). If the reader still starts the old version afterwards, it is
starting from the second slot: use the [web installer](#web-installation-via-usb)
instead, which picks the right slot for you.
