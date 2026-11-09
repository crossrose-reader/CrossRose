#pragma once

// The app deliberately has a build-time touch capability, separate from a
// runtime probe. Button-only images must not retain the touch UI merely because
// the shared source tree also builds for Sticky.
#ifndef CROSSINK_APP_CAP_TOUCH
#error "Define CROSSINK_APP_CAP_TOUCH as 0 or 1 in the PlatformIO environment"
#endif

#if CROSSINK_APP_CAP_TOUCH != 0 && CROSSINK_APP_CAP_TOUCH != 1
#error "CROSSINK_APP_CAP_TOUCH must be 0 or 1"
#endif

#ifndef CROSSINK_APP_CAP_USB_DRIVE
#error "Define CROSSINK_APP_CAP_USB_DRIVE as 0 or 1 in the PlatformIO environment"
#endif

#if CROSSINK_APP_CAP_USB_DRIVE != 0 && CROSSINK_APP_CAP_USB_DRIVE != 1
#error "CROSSINK_APP_CAP_USB_DRIVE must be 0 or 1"
#endif

// Native simulator BoardConfig intentionally exposes only simulated runtime
// profiles, so keep this firmware-image identity available at the app layer.
#if defined(FREEINK_DEVICE_X4CLASSIC) && FREEINK_DEVICE_X4CLASSIC
#define CROSSINK_APP_DEVICE_X4CLASSIC 1
#else
#define CROSSINK_APP_DEVICE_X4CLASSIC 0
#endif

// Native simulator BoardConfig deliberately has no FREEINK_CAP_TOUCH macro.
// Firmware builds must keep the app and SDK capability selections in lockstep.
#if !defined(SIMULATOR)
#include <BoardConfig.h>
#if CROSSINK_APP_CAP_TOUCH != FREEINK_CAP_TOUCH
#error "CROSSINK_APP_CAP_TOUCH must match FREEINK_CAP_TOUCH"
#endif
#if CROSSINK_APP_CAP_USB_DRIVE != FREEINK_CAP_USB_MSC
#error "CROSSINK_APP_CAP_USB_DRIVE must match FREEINK_CAP_USB_MSC"
#endif
#endif

// Every button-only reader uses the full-screen menu and its bounded sample preview.
#define CROSSINK_APP_READER_SAMPLE_PREVIEW (!CROSSINK_APP_CAP_TOUCH)

// Four-tone Home cover art runs the overlay grayscale pass over large areas
// and keeps framebuffer-sized plane copies in PSRAM. It is limited to the
// Picco until the sequence is verified on other panels; every other image
// keeps black-and-white covers.
#if defined(FREEINK_DEVICE_PICCO) && FREEINK_DEVICE_PICCO
#define CROSSINK_APP_CAP_GRAY_COVER_ART 1
#else
#define CROSSINK_APP_CAP_GRAY_COVER_ART 0
#endif
