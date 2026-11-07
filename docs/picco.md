---
title: BOOX Picco
nav_order: 15
---

# BOOX Picco

Picco has page-turn buttons, a touchscreen, a slide switch, and a frontlight
with adjustable brightness and warmth.

## Slide switch

Open **Settings > Controls > Slide Switch** and choose **Frontlight**,
**Reader Dark Mode**, **Toggle Touchscreen**, or **Tilt Page Turn**.
Moving the switch in either direction toggles that feature. The switch's
physical position does not mean on or off.

Starting, waking, or restarting the reader does not toggle the feature.
Changing the assignment also leaves it alone until you next move the switch.
Moving the switch during sleep is ignored when the reader wakes.

## Touch and frontlight

- Swipe down from the top edge to open the frontlight panel.
- While reading, swipe down anywhere to open that panel, or swipe up to open
  the Reader Menu. Use the panel's Home button to leave the book.
- Outside a book, swipe up from the bottom edge to return Home.
- Adjust brightness and warmth in the frontlight panel. Its icons follow any
  changes made with the slide switch.

Two-finger gestures appear in Controls only when the touchscreen supports them.
See [Controls](./controls.md) for pinch, rotation, and swipe options.

## Sleep and charging

Sleeping turns the frontlight off. **Settings > Display > Frontlight > Restore
Light on Wake** controls whether it comes back on when you wake the reader.
Press Power to wake it. While plugged into USB, the sleep image stays visible;
it does not show live charging updates.

## Installation and updates

Install CrossRose on the Picco over USB with
[esptool](https://docs.espressif.com/projects/esptool/) 5 or newer. The
Picco keeps its firmware in one slot, so its steps differ from the other
readers': you write only the CrossRose file, at `0xE0000`. Everything else on
the reader stays as it is, including its factory recovery software.

- Do not erase anything. Skip the `erase-region` step from the other readers'
  instructions.
- Never write at `0x10000` on a Picco. That overwrites its saved settings,
  its startup slot choice and its recovery software.
- Use only `firmware-picco-*.bin`. Files for the X4 Pro, X4 Classic or Sticky
  do not work on the Picco, and neither does a web installer's full or
  factory install.

### Install

1. Install esptool, for example with `pipx install esptool` (on macOS, run
   `brew install pipx` and `pipx ensurepath` first).
2. Download `firmware-picco-v<version>.bin`: the stable version from the
   [newest release](https://github.com/crossrose-reader/CrossRose/releases/latest),
   or a test build from the
   [Nightly release](https://github.com/crossrose-reader/CrossRose/releases/tag/nightly).
3. Connect the Picco with a USB data cable. If it is in USB Drive mode, eject
   the drive first; the port appears a few seconds later. Find the port:

   ```sh
   # Linux (for example /dev/ttyACM0)
   dmesg | grep tty

   # macOS (for example /dev/cu.usbmodem101)
   ls /dev/cu.*
   ```

   On Windows, use the COM port shown in Device Manager, such as `COM5`.

4. Optional, recommended before the first install: back up the whole flash.
   This reads all 16 MB and takes about 2 minutes. Keep the file private: it
   holds the reader's own data.

   ```sh
   esptool --chip esp32s3 --port /dev/cu.usbmodem101 read-flash 0 0x1000000 picco-backup.bin
   ```

5. Write CrossRose to the app slot. This takes about a minute.

   ```sh
   esptool --chip esp32s3 --port /dev/cu.usbmodem101 write-flash 0xE0000 firmware-picco-v<version>.bin
   ```

Replace the port and file name with your own. esptool resets the reader by
itself, so you do not need to hold any buttons. When it finishes, the Picco
restarts into CrossRose.

### Updates

Check for Updates and SD Card Firmware Update are not available on the Picco:
an update from the reader would overwrite the firmware that is running. To
update, download the newer `firmware-picco-v<version>.bin` and repeat step 5.
Your books and settings stay.
