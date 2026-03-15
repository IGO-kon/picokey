# picokey

[![build](https://github.com/IGO-kon/picokey/actions/workflows/build.yml/badge.svg)](https://github.com/IGO-kon/picokey/actions/workflows/build.yml)

BLE HID bridge firmware for Raspberry Pi Pico 2 W.

```text
Bluetooth Keyboard/Touchpad
		-> (BLE HID over GATT)
Raspberry Pi Pico 2 W
		-> (USB HID Keyboard + Mouse + CDC log)
PC
```

Current implementation is based on BTstack `hog_host_demo` (Report Mode) with
custom hooks for report parsing and gesture translation.

## Current Features

- BLE keyboard input -> USB keyboard output
- Touchpad pointer -> USB mouse output
- One-finger tap -> left click
- Two-finger tap -> right click
- Two-finger vertical movement -> wheel scroll
- Tap-and-drag (arm on tap, then drag on next touch)
- Pairing/status log via USB CDC (`/dev/ttyACM*`)
- Pico W LED status:
	- ON: pairing/re-encryption succeeded
	- OFF: disconnected or pairing failed

## Main Files

- `CMakeLists.txt`
	- Pico SDK setup
	- BTstack `hog_host_demo.c` selection
	- GATT header generation (`hog_host_demo.gatt` -> `hog_host_demo.h`)
	- Hook symbol redirection (`hids_client_connect`, `sm_set_authentication_requirements`)
- `src/main.c`
	- startup and BTstack run loop
- `src/picokey_hids_hooks.c`
	- report parser and all pointer/gesture tuning
- `src/picokey_sm_hooks.c`
	- pairing/auth requirements hardening
- `src/picokey_pairing_monitor.c`
	- CDC diagnostics and LED status control
- `src/picokey_usb_hid.c`
	- TinyUSB HID queue sender (keyboard/mouse)
- `src/usb_descriptors.c`
	- USB descriptors (CDC + HID keyboard + HID mouse)
- `src/tusb_config.h`
	- TinyUSB class/endpoint config

## Environment Setup

### VS Code Extensions (recommended)

- `ms-vscode.cpptools`
- `ms-vscode.cmake-tools`
- `twxs.cmake`
- `raspberry-pi.raspberry-pi-pico`
- `paulober.pico-w-go`

### Required Tools

- `cmake` 3.13+
- ARM GNU toolchain (`arm-none-eabi-gcc`)
- `python3` (for BTstack GATT compile step)
- `git`

Ubuntu/Debian example:

```bash
sudo apt update
sudo apt install -y cmake ninja-build gcc-arm-none-eabi libnewlib-arm-none-eabi python3 git
```

If `arm-none-eabi-gcc` is not in `PATH`, this project auto-detects the Arduino
RP2040 toolchain under `~/.arduino15/packages/rp2040/tools/pqt-gcc/*/bin`.

## Build

```bash
cmake -S . -B build -DPICO_BOARD=pico2_w
cmake --build build -j
```

Generated artifacts:

- `build/picokey.uf2`
- `build/picokey.elf`

If configure/build cache is broken, recreate `build/`:

```bash
rm -rf build
cmake -S . -B build -G "Unix Makefiles" -DPICO_BOARD=pico2_w
cmake --build build -j
```

## Flash

1. Hold `BOOTSEL` and connect Pico 2 W.
2. Copy `build/picokey.uf2` to mounted `RPI-RP2` or `RP2350` volume.
3. Device reboots automatically.

Example:

```bash
cp build/picokey.uf2 /media/$USER/RP2350/
```

## Pairing / Debug Log

```bash
screen /dev/ttyACM0 115200
```

Typical messages:

- `[PAIR] ...`
- `[HID] service connected ...`
- `[HID] tap-click`
- `[HID] two-finger-tap-right-click`

## Tuning Guide

Most behavior tuning is in `src/picokey_hids_hooks.c`.

### Pointer speed

- `PICOKEY_TOUCHPAD_SPEED_NUMERATOR`
- `PICOKEY_TOUCHPAD_SPEED_DENOMINATOR`

Lower `NUMERATOR / DENOMINATOR` makes cursor slower.

### One-finger tap (left click)

- `PICOKEY_TAP_MAX_DURATION_MS`
- `PICOKEY_TAP_MAX_TOTAL_MOTION`

Increase values to make tap easier to trigger.

### Two-finger tap (right click)

- `PICOKEY_TWO_FINGER_TAP_MAX_DURATION_MS`
- `PICOKEY_TWO_FINGER_TAP_MAX_TOTAL_MOTION`

Increase values to make right-click tap easier.

### Staggered two-finger landing (timing tolerance)

- `PICOKEY_TWO_FINGER_JOIN_MAX_DELAY_MS`
- `PICOKEY_TWO_FINGER_JOIN_MAX_TOTAL_MOTION`

These control tolerance for `finger1 then finger2 shortly after` behavior.

### Tap-and-drag

- `PICOKEY_DRAG_ARM_TIMEOUT_MS`

Larger value keeps drag-arming alive longer after a tap-click.

### Two-finger scroll feel

- `PICOKEY_SCROLL_DELTA_PER_STEP`

Smaller value makes wheel events fire more often (faster scrolling).

Direction inversion is done in the two-finger path where
`tracker->scroll_remainder` is updated.

## Notes

- `src/btstack_hooks.c` and `src/picokey_bt_hooks.h` are legacy boot-mode path
	artifacts and are not used by current CMake target.
- USB exposes both HID and CDC interfaces.
- If CMake Tools extension shows generic configure failure, check terminal
	`cmake -S . -B build` output directly for actionable diagnostics.

## Community

- License: `LICENSE` (MIT)
- Contributing guide: `CONTRIBUTING.md`
- Code of Conduct: `CODE_OF_CONDUCT.md`
- Security reporting: `SECURITY.md`
- Bug/feature templates: `.github/ISSUE_TEMPLATE/`
- Pull request template: `.github/PULL_REQUEST_TEMPLATE.md`

## Maintainer Publish Checklist

If you want this repository to be easily usable by others, check these items:

1. Set repository visibility to Public in GitHub settings.
2. Keep Issues and Pull Requests enabled.
3. Enable Discussions if you want community Q and A.
4. Add repository topics, for example: `pico2w`, `bluetooth`, `hid`, `touchpad`, `firmware`.
5. Keep CI green (`.github/workflows/build.yml`).
6. Tag stable versions and publish Releases with notes and `picokey.uf2` artifact.