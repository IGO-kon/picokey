# picokey

Bluetooth keyboard bridge for Raspberry Pi Pico 2 W.

```text
Bluetooth Keyboard
	-> (Bluetooth HID)
Raspberry Pi Pico 2 W
	-> (USB HID Keyboard)
PC
```

This firmware runs BTstack `hog_boot_host_demo` on Pico 2 W and forwards
received Boot Keyboard reports to USB HID keyboard reports.

## Included Setup

- `CMakeLists.txt`: build config for Pico SDK + BTstack HOG host demo + TinyUSB HID.
- `pico_sdk_import.cmake`: auto-loads `PICO_SDK_PATH` or fetches Pico SDK from Git.
- `src/main.c`: Pico W startup wrapper for BTstack event loop.
- `src/btstack_config.h`: BTstack runtime configuration.
- `src/picokey_usb_hid.c`: TinyUSB HID send queue.
- `src/usb_descriptors.c`: USB device/HID descriptors.
- `.vscode/settings.json`: CMake Tools defaults (`PICO_BOARD=pico2_w`).

## Prerequisites

- `cmake` 3.13+
- ARM GNU toolchain (`arm-none-eabi-gcc`)
- `git` (needed when SDK is fetched automatically)

If `arm-none-eabi-gcc` is not in `PATH`, this project tries to auto-detect the
Arduino RP2040 toolchain under `~/.arduino15/...`.

Ubuntu/Debian example:

```bash
sudo apt update
sudo apt install -y cmake ninja-build gcc-arm-none-eabi libnewlib-arm-none-eabi git
```

## Build

```bash
cmake -S . -B build -DPICO_BOARD=pico2_w
cmake --build build -j
```

If compiler detection fails, set it explicitly:

```bash
cmake -S . -B build -DPICO_BOARD=pico2_w -DPICO_TOOLCHAIN_PATH=$HOME/.arduino15/packages/rp2040/tools/pqt-gcc/4.1.0-1aec55e/bin
```

If you see `CMAKE_MAKE_PROGRAM is not set` with `Ninja`, your existing
`build/` cache is likely pinned to `Ninja`. Recreate it:

```bash
rm -rf build
cmake -S . -B build -G "Unix Makefiles" -DPICO_BOARD=pico2_w
cmake --build build -j
```

Generated firmware:

- UF2: `build/picokey.uf2`
- ELF: `build/picokey.elf`

## Run

1. Hold `BOOTSEL` while connecting Pico 2 W via USB.
2. Copy `build/picokey.uf2` to `RPI-RP2`.
3. Reconnect board normally.
4. Put your Bluetooth keyboard in pairing mode.
5. Open pairing log (USB CDC):

```bash
screen /dev/ttyACM0 115200
```

6. If prompted, type the shown PIN on your Bluetooth keyboard.
7. The firmware pairs with HID keyboard devices.
8. The PC recognizes Pico 2 W as a USB keyboard and receives key input.

## Notes

- USB output is keyboard HID only in the current stable configuration.
- Touchpad/mouse forwarding is not enabled in this checkpoint.
- Media keys and full report-map translation are not implemented yet.
- USB exposes both HID keyboard and CDC log interface.
- Onboard LED status:
	- ON: pairing/re-encryption completed (link secured)
	- OFF: disconnected or pairing failed