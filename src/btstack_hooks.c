#include <stdbool.h>
#include <stdint.h>

#include "btstack.h"
#include "picokey_bt_hooks.h"
#include "picokey_usb_hid.h"

static gatt_client_notification_t s_usb_hid_keyboard_notification;
static gatt_client_notification_t s_usb_hid_mouse_notification;
static btstack_packet_callback_registration_t s_hci_event_callback_registration;
static bool s_usb_keyboard_listener_registered = false;
static bool s_usb_mouse_listener_registered = false;
static hci_con_handle_t s_connection_handle = HCI_CON_HANDLE_INVALID;
static uint16_t s_keyboard_value_handle = 0;
static uint16_t s_mouse_value_handle = 0;

static void picokey_hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    (void)channel;
    (void)size;

    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }

    if (hci_event_packet_get_type(packet) != HCI_EVENT_DISCONNECTION_COMPLETE) {
        return;
    }

    hci_con_handle_t disconnected = hci_event_disconnection_complete_get_connection_handle(packet);
    if (disconnected != s_connection_handle) {
        return;
    }

    s_connection_handle = HCI_CON_HANDLE_INVALID;
    s_keyboard_value_handle = 0;
    s_mouse_value_handle = 0;

    if (s_usb_keyboard_listener_registered) {
        gatt_client_stop_listening_for_characteristic_value_updates(&s_usb_hid_keyboard_notification);
        s_usb_keyboard_listener_registered = false;
    }

    if (s_usb_mouse_listener_registered) {
        gatt_client_stop_listening_for_characteristic_value_updates(&s_usb_hid_mouse_notification);
        s_usb_mouse_listener_registered = false;
    }

    picokey_usb_hid_release_all();
}

static void picokey_boot_keyboard_notification_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet,
                                                       uint16_t size)
{
    (void)channel;
    (void)size;

    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }
    if (hci_event_packet_get_type(packet) != GATT_EVENT_NOTIFICATION) {
        return;
    }

    uint16_t value_handle = gatt_event_notification_get_value_handle(packet);
    if (value_handle != s_keyboard_value_handle) {
        return;
    }

    const uint8_t *data = gatt_event_notification_get_value(packet);
    uint16_t len = gatt_event_notification_get_value_length(packet);
    if (len < 8) {
        return;
    }

    picokey_usb_hid_send_keyboard_report(data[0], &data[2]);
}

static void picokey_boot_mouse_notification_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet,
                                                    uint16_t size)
{
    (void)channel;
    (void)size;

    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }
    if (hci_event_packet_get_type(packet) != GATT_EVENT_NOTIFICATION) {
        return;
    }

    uint16_t value_handle = gatt_event_notification_get_value_handle(packet);
    if (value_handle != s_mouse_value_handle) {
        return;
    }

    const uint8_t *data = gatt_event_notification_get_value(packet);
    uint16_t len = gatt_event_notification_get_value_length(packet);
    if (len < 3) {
        return;
    }

    int8_t x = (int8_t)data[1];
    int8_t y = (int8_t)data[2];
    int8_t wheel = 0;
    if (len >= 4) {
        wheel = (int8_t)data[3];
    }

    picokey_usb_hid_send_mouse_report(data[0], x, y, wheel);
}

void picokey_gatt_client_listen_for_characteristic_value_updates(
    gatt_client_notification_t *notification,
    btstack_packet_handler_t callback,
    hci_con_handle_t con_handle,
    gatt_client_characteristic_t *characteristic)
{
    // Register original listener first so demo behavior is preserved.
    gatt_client_listen_for_characteristic_value_updates(notification, callback, con_handle, characteristic);

    if (characteristic == NULL) {
        return;
    }

    bool is_keyboard = characteristic->uuid16 == ORG_BLUETOOTH_CHARACTERISTIC_BOOT_KEYBOARD_INPUT_REPORT;
    bool is_mouse = characteristic->uuid16 == ORG_BLUETOOTH_CHARACTERISTIC_BOOT_MOUSE_INPUT_REPORT;
    if (!is_keyboard && !is_mouse) {
        return;
    }

    s_connection_handle = con_handle;

    if (is_keyboard) {
        s_keyboard_value_handle = characteristic->value_handle;

        if (s_usb_keyboard_listener_registered) {
            gatt_client_stop_listening_for_characteristic_value_updates(&s_usb_hid_keyboard_notification);
            s_usb_keyboard_listener_registered = false;
        }

        gatt_client_listen_for_characteristic_value_updates(
            &s_usb_hid_keyboard_notification,
            picokey_boot_keyboard_notification_handler,
            con_handle,
            characteristic);
        s_usb_keyboard_listener_registered = true;
    } else {
        s_mouse_value_handle = characteristic->value_handle;

        if (s_usb_mouse_listener_registered) {
            gatt_client_stop_listening_for_characteristic_value_updates(&s_usb_hid_mouse_notification);
            s_usb_mouse_listener_registered = false;
        }

        gatt_client_listen_for_characteristic_value_updates(
            &s_usb_hid_mouse_notification,
            picokey_boot_mouse_notification_handler,
            con_handle,
            characteristic);
        s_usb_mouse_listener_registered = true;
    }

    if (s_hci_event_callback_registration.callback == NULL) {
        s_hci_event_callback_registration.callback = &picokey_hci_event_handler;
        hci_add_event_handler(&s_hci_event_callback_registration);
    }
}
