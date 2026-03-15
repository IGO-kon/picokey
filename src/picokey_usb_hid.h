#ifndef PICOKEY_USB_HID_H
#define PICOKEY_USB_HID_H

#include <stdint.h>

void picokey_usb_hid_init(void);
void picokey_usb_hid_task(void);
void picokey_usb_hid_send_keyboard_report(uint8_t modifier, const uint8_t keycode[6]);
void picokey_usb_hid_send_mouse_report(uint8_t buttons, int8_t x, int8_t y, int8_t wheel);
void picokey_usb_hid_release_all(void);

#endif
