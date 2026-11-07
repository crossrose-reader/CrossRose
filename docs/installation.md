---
title: Installation
nav_order: 2
---

# Installation

## Supported Devices

- Xteink X3 and X4
- Xteink X4 Classic and X4 Pro
- Seeed Studio Sticky
- Boox Picco: its install steps are different. See [BOOX Picco](./picco.md#installation-and-updates).

Don't have a device yet? Get one directly from [Xteink](https://go.sjv.io/X4RGBb) or [Seeed Studio](https://www.seeedstudio.com/reTerminal-Sticky-p-6861.html?sensecap_affiliate=1Nxo3Gw&referring_service=link).

Note: Your purchases using the above affiliate links help support ongoing development of Crossink.

## Languages and hyphenation

You can add menu languages and hyphenation packs without erasing your reader.
Some devices keep installed packs on the SD card, so keep that card in the
reader. See [Languages](./languages.md) for setup and recovery.

## Which File Do I Need?

CrossRose has two kinds of releases:

- **Stable** ([latest stable release](https://github.com/crossrose-reader/CrossRose/releases/latest)): tested releases, numbered like `v0.1.0`. Version 1.0.0 will be the first big release. Pick this one if you are not sure.
- **Nightly** ([nightly release](https://github.com/crossrose-reader/CrossRose/releases/tag/nightly)): the newest code, built every night. For testing. It can have bugs.

Until the first stable release (`v0.1.0`) comes out, there is no stable
firmware: the stable link opens the releases list, which shows only the
nightly. Use the nightly until then.

Each release has one `firmware-*.bin` file per device. Download the one for your device:

| Device | How to tell | File |
| --- | --- | --- |
| Xteink X4 | Up/Down buttons on the right side, USB-C port | `firmware-x3-x4-*.bin` |
| Xteink X3 | Up/Down buttons on both sides, POGO port, power button on top | `firmware-x3-x4-*.bin` |
| Xteink X4 Classic (also called X4 V2) | Up/Down buttons on both sides, POGO port, power button on the right side | `firmware-x4-classic-*.bin` |
| Xteink X4 Pro | Sold by Xteink, with a touchscreen, a front light and a Home key | `firmware-x4-pro-*.bin` |
| Seeed Studio Sticky | Sold by Seeed Studio as the reTerminal Sticky | `firmware-sticky-*.bin` |
| Boox Picco | Sold by Boox, with a slide switch | `firmware-picco-*.bin`, installed with the [BOOX Picco](./picco.md#installation-and-updates) steps |

Check the file name before you install. A file made for another reader can
damage it. The web installer only checks the chip type, so it cannot tell the
X4 Pro, X4 Classic, Sticky and Picco files apart. The SD card update in
CrossRose, and in CrossInk 1.5.1 or newer, refuses a file made for another
reader.

To go back to an earlier stable version, download that version's file from the
[releases page](https://github.com/crossrose-reader/CrossRose/releases) and
install it the same way. Only the newest nightly is kept. To go back to
CrossInk, install it with [Inky](https://inky.crossink.dev/#flash-tools).

## Web Installation via USB

#### For new installs and updates.

Inky, CrossInk's web app, can only install CrossInk. To install CrossRose over
USB, use the CrossPoint web installer. CrossPoint is the project that CrossInk
and CrossRose are built on. Its installer can flash a `.bin` file from your
computer.

Boox Picco: do not use the web installer. See [BOOX Picco](./picco.md#installation-and-updates).

1. Download the `firmware-*.bin` file for your device (see [Which File Do I Need?](#which-file-do-i-need)).
2. Open [https://crosspointreader.com/install](https://crosspointreader.com/install) in Chrome or Edge on a computer.
3. Select your device. The X4 Classic is listed as **Xteink X4C**.
4. Open **Advanced options** and click **Custom .bin**. Then click **Choose firmware file…** (next to the **Flash** button) and pick the file you downloaded.
5. Connect the reader with a USB cable that carries data, not only power. The X3 and X4 Classic connect through their POGO port. Click **Flash**, choose your reader in the browser's list, and click **Connect**.
6. Keep the cable connected until the page says the firmware is installed. Then follow the restart steps on the page.

If the reader does not show up in the browser's list, try another cable or USB
port. If it still does not show up, USB may be turned off on your device: see
[USB Locked Devices](#usb-locked-devices).

## USB Drive

On the X4 Classic, X4 Pro and Boox Picco, you can plug the reader into your
computer and use its SD card like a USB stick. This is handy for copying books
and fonts.

1. On the reader, go to **Home > File Transfer > USB Drive**.
2. Connect the reader to your computer with a USB cable that carries data, not only power. The X4 Classic connects through its POGO port.
3. When you are done, eject the drive on your computer first, then unplug the cable.

The reader restarts to the Home screen once the drive is ejected or the cable
is removed.

## SD Card Firmware Update

#### For updating CrossRose, or switching from CrossInk or CrossPoint to CrossRose. Can be used by USB locked devices.

Use this when your reader already runs CrossRose, CrossInk 1.2.10 or newer, or
CrossPoint. The Boox Picco has neither this nor Check for Updates: to update a
Picco, see [BOOX Picco](./picco.md#updates).

1. Download the `firmware-*.bin` file for your device (see [Which File Do I Need?](#which-file-do-i-need)).
2. Place the file on your SD card. You can place this file anywhere.
3. Go to **Settings > System > SD Card Firmware Update** and navigate to the `.bin` file and update.
4. Do not turn the reader off until the update finishes.

If your reader already runs CrossRose, **Settings > System > Check for Updates**
can also install the newest stable CrossRose release over Wi-Fi. It does not
install nightly builds.

If your reader is not a Picco and **SD Card Firmware Update** is not in the
menu, the firmware on your reader is too old. Update it first with
**Settings > System > Check for Updates**, then try again.

## USB Locked Devices

Some readers have USB data transfer disabled. The web installer cannot reach
them, so use the SD card instead.

**If the reader already runs CrossRose, CrossInk or CrossPoint:** follow
[SD Card Firmware Update](#sd-card-firmware-update).

**If the reader still runs the stock Xteink firmware:** install CrossInk first,
then switch to CrossRose.

1. Go to [Inky](https://inky.crossink.dev/#flash-tools), check "I have a locked device", select your device, and follow the steps to install CrossInk. Pick a stable version, not a pre-release. On the X4 Pro and X4 Classic, Inky first sends you to the [OTA Unlocker](https://crosspointreader.com/unlocker).
2. Follow [SD Card Firmware Update](#sd-card-firmware-update) to install CrossRose.

CrossInk is marked "safe" on [Brick Club](https://brickclub.pages.dev/), a
community list that rates how safely each Xteink firmware flashes on locked X3
and X4 readers. CrossRose is not on that list yet, so this guide takes the
two-step way.

## Command Line

These instructions are for macOS and Linux, on the X3, X4, X4 Pro and X4
Classic. Windows users and Sticky owners should use the
[web installer](#web-installation-via-usb). On the Sticky, the first install
also replaces the bootloader and partition table, which these commands do not do.

Boox Picco: the steps are different. See [BOOX Picco](./picco.md#installation-and-updates).

Install `esptool` 5 or newer with [pipx](https://pipx.pypa.io/):

```sh
pipx install esptool
```

If you don't have pipx, install it with `brew install pipx` on macOS or
`sudo apt install pipx` on Debian and Ubuntu. Then run `pipx ensurepath` and
open a new terminal.

Download the `firmware-*.bin` file for your device (see [Which File Do I Need?](#which-file-do-i-need)), then connect your device with a USB data cable. The X3 and X4 Classic connect through their POGO port.

Find the device port:

```sh
# Linux
ls /dev/ttyACM*

# macOS
ls /dev/cu.*
```

The reader keeps firmware in two slots, and an update can leave it starting
from the second one. The first command below writes the firmware to the first
slot (`0x10000`). The second clears the slot choice, so the reader starts from
the first slot. Your books and settings stay.

```sh
# Linux
esptool --chip esp32c3 --port /dev/ttyACM0 --baud 921600 write-flash 0x10000 /path/to/firmware.bin
esptool --chip esp32c3 --port /dev/ttyACM0 erase-region 0xe000 0x2000

# macOS
esptool --chip esp32c3 --port /dev/cu.usbmodem2101 --baud 921600 write-flash 0x10000 /path/to/firmware.bin
esptool --chip esp32c3 --port /dev/cu.usbmodem2101 erase-region 0xe000 0x2000
```

Replace the port and firmware path with your actual values. On Linux, if
esptool says `Permission denied`, run `sudo usermod -aG dialout $USER`, then log
out and back in.

The X4 Pro and X4 Classic use an ESP32-S3 chip: write `--chip esp32s3` instead
of `--chip esp32c3`.

When the commands finish, the reader restarts into CrossRose by itself. Unplug
the cable once it shows the Home screen or the book you were reading.
