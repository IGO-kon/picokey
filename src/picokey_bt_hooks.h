#ifndef PICOKEY_BT_HOOKS_H
#define PICOKEY_BT_HOOKS_H

#include "ble/gatt_client.h"

void picokey_gatt_client_listen_for_characteristic_value_updates(
    gatt_client_notification_t *notification,
    btstack_packet_handler_t callback,
    hci_con_handle_t con_handle,
    gatt_client_characteristic_t *characteristic);

#endif
