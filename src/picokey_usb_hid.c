#include <stdbool.h>
#include <string.h>

#include "picokey_usb_reports.h"
#include "picokey_usb_hid.h"
#include "tusb.h"

typedef struct {
    uint8_t modifier;
    uint8_t keycode[6];
} picokey_keyboard_report_t;

#define PICOKEY_HID_EVENT_QUEUE_CAPACITY 32

static picokey_keyboard_report_t s_hid_event_queue[PICOKEY_HID_EVENT_QUEUE_CAPACITY];
static uint8_t s_hid_event_queue_head;
static uint8_t s_hid_event_queue_tail;
static uint8_t s_hid_event_queue_count;

static bool picokey_usb_hid_enqueue_event(const picokey_keyboard_report_t *event)
{
    if (s_hid_event_queue_count >= PICOKEY_HID_EVENT_QUEUE_CAPACITY) {
        return false;
    }

    s_hid_event_queue[s_hid_event_queue_tail] = *event;
    s_hid_event_queue_tail = (uint8_t)((s_hid_event_queue_tail + 1u) % PICOKEY_HID_EVENT_QUEUE_CAPACITY);
    s_hid_event_queue_count++;
    return true;
}

static void picokey_usb_hid_try_flush(void)
{
    while (s_hid_event_queue_count > 0u) {
        const picokey_keyboard_report_t *event = &s_hid_event_queue[s_hid_event_queue_head];

        if (!tud_hid_n_ready(PICOKEY_USB_HID_ITF_KEYBOARD)) {
            return;
        }

        if (!tud_hid_n_keyboard_report(PICOKEY_USB_HID_ITF_KEYBOARD, 0, event->modifier, event->keycode)) {
            return;
        }

        s_hid_event_queue_head = (uint8_t)((s_hid_event_queue_head + 1u) % PICOKEY_HID_EVENT_QUEUE_CAPACITY);
        s_hid_event_queue_count--;
    }
}

void picokey_usb_hid_init(void)
{
    memset(s_hid_event_queue, 0, sizeof(s_hid_event_queue));
    s_hid_event_queue_head = 0;
    s_hid_event_queue_tail = 0;
    s_hid_event_queue_count = 0;
    tusb_init();
}

void picokey_usb_hid_task(void)
{
    tud_task();
    picokey_usb_hid_try_flush();
}

void picokey_usb_hid_send_keyboard_report(uint8_t modifier, const uint8_t keycode[6])
{
    picokey_keyboard_report_t event;
    memset(&event, 0, sizeof(event));
    event.modifier = modifier;
    memcpy(event.keycode, keycode, sizeof(event.keycode));

    if (!picokey_usb_hid_enqueue_event(&event)) {
        return;
    }

    picokey_usb_hid_try_flush();
}

void picokey_usb_hid_send_mouse_report(uint8_t buttons, int8_t x, int8_t y, int8_t wheel)
{
    (void)buttons;
    (void)x;
    (void)y;
    (void)wheel;
}

void picokey_usb_hid_release_all(void)
{
    static const uint8_t released[6] = {0, 0, 0, 0, 0, 0};
    picokey_usb_hid_send_keyboard_report(0, released);
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           const uint8_t *buffer, uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}
