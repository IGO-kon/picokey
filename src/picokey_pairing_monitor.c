#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "btstack.h"
#include "class/cdc/cdc_device.h"
#include "pico/cyw43_arch.h"

static btstack_packet_callback_registration_t s_sm_event_callback_registration;
static btstack_packet_callback_registration_t s_hci_event_callback_registration;

static void picokey_pair_led_set(bool on)
{
    (void)cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on ? 1 : 0);
}

static void picokey_cdc_log(const char *fmt, ...)
{
    if (!tud_cdc_connected()) {
        return;
    }

    char line[160];
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    if (written <= 0) {
        return;
    }

    size_t len = (size_t)written;
    if (len >= sizeof(line)) {
        len = sizeof(line) - 1;
    }

    tud_cdc_write(line, len);
    tud_cdc_write_flush();
}

static void picokey_sm_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    (void)channel;
    (void)size;

    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }

    switch (hci_event_packet_get_type(packet)) {
    case SM_EVENT_JUST_WORKS_REQUEST:
        picokey_cdc_log("[PAIR] Just Works requested\r\n");
        break;
    case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
        picokey_cdc_log("[PAIR] Numeric comparison: %06" PRIu32 " (auto confirm)\r\n",
                        sm_event_numeric_comparison_request_get_passkey(packet));
        break;
    case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
        picokey_cdc_log("[PAIR] Type this PIN on BT keyboard: %06" PRIu32 "\r\n",
                        sm_event_passkey_display_number_get_passkey(packet));
        break;
    case SM_EVENT_PASSKEY_INPUT_NUMBER:
        picokey_cdc_log("[PAIR] Host passkey input requested (not supported)\r\n");
        break;
    case SM_EVENT_PAIRING_COMPLETE:
        if (sm_event_pairing_complete_get_status(packet) == ERROR_CODE_SUCCESS) {
            picokey_pair_led_set(true);
            picokey_cdc_log("[PAIR] Pairing complete\r\n");
        } else {
            picokey_pair_led_set(false);
            picokey_cdc_log("[PAIR] Failed status=%u reason=%u\r\n",
                            sm_event_pairing_complete_get_status(packet),
                            sm_event_pairing_complete_get_reason(packet));
        }
        break;
    case SM_EVENT_REENCRYPTION_COMPLETE:
        if (sm_event_reencryption_complete_get_status(packet) == ERROR_CODE_SUCCESS) {
            picokey_pair_led_set(true);
            picokey_cdc_log("[PAIR] Re-encryption complete\r\n");
        } else {
            picokey_pair_led_set(false);
            picokey_cdc_log("[PAIR] Re-encryption failed status=%u\r\n",
                            sm_event_reencryption_complete_get_status(packet));
        }
        break;
    default:
        break;
    }
}

static void picokey_hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    (void)channel;
    (void)size;

    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }

    if (hci_event_packet_get_type(packet) == HCI_EVENT_DISCONNECTION_COMPLETE) {
        picokey_pair_led_set(false);
        picokey_cdc_log("[PAIR] Disconnected\r\n");
    }
}

void picokey_pairing_monitor_init(void)
{
    picokey_pair_led_set(false);

    s_sm_event_callback_registration.callback = &picokey_sm_event_handler;
    sm_add_event_handler(&s_sm_event_callback_registration);

    s_hci_event_callback_registration.callback = &picokey_hci_event_handler;
    hci_add_event_handler(&s_hci_event_callback_registration);
}
