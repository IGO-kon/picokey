#include <stdbool.h>
#include <string.h>

#include "picokey_usb_reports.h"
#include "picokey_usb_hid.h"
#include "tusb.h"

typedef struct {
    uint8_t modifier;
    uint8_t keycode[6];
} picokey_keyboard_report_t;

typedef struct {
    uint8_t buttons;
    int8_t x;
    int8_t y;
    int8_t wheel;
} picokey_mouse_report_t;

typedef enum {
    PICOKEY_HID_EVENT_KEYBOARD,
    PICOKEY_HID_EVENT_MOUSE,
} picokey_hid_event_type_t;

typedef struct {
    picokey_hid_event_type_t type;
    union {
        picokey_keyboard_report_t keyboard;
        picokey_mouse_report_t mouse;
    } data;
} picokey_hid_event_t;

#define PICOKEY_HID_EVENT_QUEUE_CAPACITY 32

static picokey_hid_event_t s_hid_event_queue[PICOKEY_HID_EVENT_QUEUE_CAPACITY];
static uint8_t s_hid_event_queue_head;
static uint8_t s_hid_event_queue_tail;
static uint8_t s_hid_event_queue_count;

static bool picokey_usb_hid_enqueue_event(const picokey_hid_event_t *event)
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
        const picokey_hid_event_t *event = &s_hid_event_queue[s_hid_event_queue_head];
        bool sent = false;

        switch (event->type) {
        case PICOKEY_HID_EVENT_KEYBOARD:
            if (!tud_hid_n_ready(PICOKEY_USB_HID_ITF_KEYBOARD)) {
                return;
            }
            sent = tud_hid_n_keyboard_report(PICOKEY_USB_HID_ITF_KEYBOARD, 0, event->data.keyboard.modifier,
                                             event->data.keyboard.keycode);
            break;
        case PICOKEY_HID_EVENT_MOUSE:
            if (!tud_hid_n_ready(PICOKEY_USB_HID_ITF_MOUSE)) {
                return;
            }
            sent = tud_hid_n_mouse_report(PICOKEY_USB_HID_ITF_MOUSE, 0, event->data.mouse.buttons,
                                          event->data.mouse.x, event->data.mouse.y, event->data.mouse.wheel, 0);
            break;
        default:
            break;
        }

        if (!sent) {
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
    picokey_hid_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = PICOKEY_HID_EVENT_KEYBOARD;
    event.data.keyboard.modifier = modifier;
    memcpy(event.data.keyboard.keycode, keycode, sizeof(event.data.keyboard.keycode));

    if (!picokey_usb_hid_enqueue_event(&event)) {
        return;
    }

    picokey_usb_hid_try_flush();
}

void picokey_usb_hid_send_mouse_report(uint8_t buttons, int8_t x, int8_t y, int8_t wheel)
{
    picokey_hid_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = PICOKEY_HID_EVENT_MOUSE;
    event.data.mouse.buttons = buttons;
    event.data.mouse.x = x;
    event.data.mouse.y = y;
    event.data.mouse.wheel = wheel;

    if (!picokey_usb_hid_enqueue_event(&event)) {
        return;
    }

    picokey_usb_hid_try_flush();
}

void picokey_usb_hid_release_all(void)
{
    static const uint8_t released[6] = {0, 0, 0, 0, 0, 0};
    picokey_usb_hid_send_keyboard_report(0, released);
    picokey_usb_hid_send_mouse_report(0, 0, 0, 0);
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
