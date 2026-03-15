#include <stdbool.h>
#include <stdint.h>

#include "btstack.h"
#include "picokey_bt_hooks.h"
#include "picokey_usb_hid.h"

static gatt_client_notification_t s_usb_hid_keyboard_notification;
static btstack_packet_callback_registration_t s_hci_event_callback_registration;
static bool s_usb_keyboard_listener_registered = false;
static hci_con_handle_t s_connection_handle = HCI_CON_HANDLE_INVALID;
static uint16_t s_keyboard_value_handle = 0;

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

    if (s_usb_keyboard_listener_registered) {
        gatt_client_stop_listening_for_characteristic_value_updates(&s_usb_hid_keyboard_notification);
        s_usb_keyboard_listener_registered = false;
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
    if (characteristic->uuid16 != ORG_BLUETOOTH_CHARACTERISTIC_BOOT_KEYBOARD_INPUT_REPORT) {
        return;
    }

    s_connection_handle = con_handle;

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

    if (s_hci_event_callback_registration.callback == NULL) {
        s_hci_event_callback_registration.callback = &picokey_hci_event_handler;
        hci_add_event_handler(&s_hci_event_callback_registration);
    }
}
