#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "btstack.h"
#include "btstack_hid_parser.h"
#include "ble/gatt-service/hids_client.h"
#include "picokey_pairing_monitor.h"
#include "picokey_usb_hid.h"

#define PICOKEY_MAX_POINTER_TRACKERS 8
#define PICOKEY_TOUCHPAD_SPEED_NUMERATOR 13
#define PICOKEY_TOUCHPAD_SPEED_DENOMINATOR 16
#define PICOKEY_TAP_MAX_DURATION_MS 180u
#define PICOKEY_TAP_MAX_TOTAL_MOTION 10
#define PICOKEY_TWO_FINGER_TAP_MAX_DURATION_MS 230u
#define PICOKEY_TWO_FINGER_TAP_MAX_TOTAL_MOTION 15
#define PICOKEY_TWO_FINGER_JOIN_MAX_DELAY_MS 130u
#define PICOKEY_TWO_FINGER_JOIN_MAX_TOTAL_MOTION 10
#define PICOKEY_DRAG_ARM_TIMEOUT_MS 280u
#define PICOKEY_SCROLL_DELTA_PER_STEP 12

typedef struct {
    bool in_use;
    uint16_t hids_cid;
    uint8_t service_index;
    uint8_t report_id;
    bool have_x;
    bool have_y;
    bool touch_active;
    bool drag_active;
    bool drag_arm_pending;
    bool drag_arm_active_touch;
    bool tap_candidate;
    bool two_finger_tap_candidate;
    bool two_finger_join_pending;
    uint32_t tap_start_ms;
    uint32_t two_finger_tap_start_ms;
    uint32_t two_finger_join_expire_ms;
    uint32_t drag_arm_expire_ms;
    int32_t tap_motion_accum;
    int32_t two_finger_tap_motion_accum;
    int32_t two_finger_join_motion_accum;
    uint8_t last_touch_contact_count;
    int32_t scroll_remainder;
    int32_t last_x;
    int32_t last_y;
} picokey_pointer_tracker_t;

static btstack_packet_handler_t s_app_hids_packet_handler;
static picokey_pointer_tracker_t s_pointer_trackers[PICOKEY_MAX_POINTER_TRACKERS];
static uint32_t s_hid_report_counter;

static void picokey_reset_pointer_trackers(void)
{
    memset(s_pointer_trackers, 0, sizeof(s_pointer_trackers));
}

static void picokey_hid_log_report_brief(uint8_t service_index, uint8_t report_id, const uint8_t *report, uint16_t report_len)
{
    // Keep logs sparse to avoid flooding CDC while still proving data flow.
    if ((s_hid_report_counter % 20u) != 0u) {
        return;
    }

    uint8_t b0 = 0;
    uint8_t b1 = 0;
    uint8_t b2 = 0;
    if (report_len > 0u) {
        b0 = report[0];
    }
    if (report_len > 1u) {
        b1 = report[1];
    }
    if (report_len > 2u) {
        b2 = report[2];
    }

    picokey_pairing_monitor_log("[HID] rpt svc=%u id=%u len=%u b0=%02x b1=%02x b2=%02x\r\n", service_index, report_id,
                               report_len, b0, b1, b2);
}

static picokey_pointer_tracker_t *picokey_get_pointer_tracker(uint16_t hids_cid, uint8_t service_index,
                                                              uint8_t report_id, bool create_if_missing)
{
    picokey_pointer_tracker_t *free_slot = NULL;

    for (uint8_t i = 0; i < PICOKEY_MAX_POINTER_TRACKERS; i++) {
        picokey_pointer_tracker_t *tracker = &s_pointer_trackers[i];
        if (!tracker->in_use) {
            if (free_slot == NULL) {
                free_slot = tracker;
            }
            continue;
        }

        if ((tracker->hids_cid == hids_cid) && (tracker->service_index == service_index) &&
            (tracker->report_id == report_id)) {
            return tracker;
        }
    }

    if (!create_if_missing || (free_slot == NULL)) {
        return NULL;
    }

    free_slot->in_use = true;
    free_slot->hids_cid = hids_cid;
    free_slot->service_index = service_index;
    free_slot->report_id = report_id;
    free_slot->have_x = false;
    free_slot->have_y = false;
    free_slot->touch_active = false;
    free_slot->drag_active = false;
    free_slot->drag_arm_pending = false;
    free_slot->drag_arm_active_touch = false;
    free_slot->tap_candidate = false;
    free_slot->two_finger_tap_candidate = false;
    free_slot->two_finger_join_pending = false;
    free_slot->tap_start_ms = 0;
    free_slot->two_finger_tap_start_ms = 0;
    free_slot->two_finger_join_expire_ms = 0;
    free_slot->drag_arm_expire_ms = 0;
    free_slot->tap_motion_accum = 0;
    free_slot->two_finger_tap_motion_accum = 0;
    free_slot->two_finger_join_motion_accum = 0;
    free_slot->last_touch_contact_count = 0;
    free_slot->scroll_remainder = 0;
    free_slot->last_x = 0;
    free_slot->last_y = 0;
    return free_slot;
}

static int8_t picokey_clamp_i8(int32_t value)
{
    if (value > 127) {
        return 127;
    }
    if (value < -127) {
        return -127;
    }
    return (int8_t)value;
}

static int32_t picokey_scale_touchpad_delta(int32_t value)
{
    // Keep response natural: slightly slower (~13/16) but never swallow tiny movement.
    int32_t scaled = (value * PICOKEY_TOUCHPAD_SPEED_NUMERATOR) / PICOKEY_TOUCHPAD_SPEED_DENOMINATOR;
    if ((value != 0) && (scaled == 0)) {
        return (value > 0) ? 1 : -1;
    }
    return scaled;
}

static int32_t picokey_abs32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static int32_t picokey_extract_scroll_steps(int32_t *remainder)
{
    int32_t steps = 0;
    while (*remainder >= PICOKEY_SCROLL_DELTA_PER_STEP) {
        (*remainder) -= PICOKEY_SCROLL_DELTA_PER_STEP;
        steps++;
    }
    while (*remainder <= -PICOKEY_SCROLL_DELTA_PER_STEP) {
        (*remainder) += PICOKEY_SCROLL_DELTA_PER_STEP;
        steps--;
    }
    return steps;
}

static bool picokey_report_has_explicit_report_id(const uint8_t *descriptor, uint16_t descriptor_len)
{
    btstack_hid_descriptor_iterator_t iterator;
    btstack_hid_descriptor_iterator_init(&iterator, descriptor, descriptor_len);

    while (btstack_hid_descriptor_iterator_has_more(&iterator)) {
        const hid_descriptor_item_t *item = btstack_hid_descriptor_iterator_get_item(&iterator);
        if ((item->item_type == Global) && (item->item_tag == ReportID)) {
            return true;
        }
    }

    return false;
}

static void picokey_forward_hid_report(uint16_t hids_cid, uint8_t service_index, uint8_t report_id, const uint8_t *report,
                                       uint16_t report_len)
{
    if ((report == NULL) || (report_len == 0)) {
        return;
    }

    const uint8_t *descriptor = hids_client_descriptor_storage_get_descriptor_data(hids_cid, service_index);
    uint16_t descriptor_len = hids_client_descriptor_storage_get_descriptor_len(hids_cid, service_index);
    if ((descriptor == NULL) || (descriptor_len == 0)) {
        return;
    }

    const bool descriptor_uses_report_id = picokey_report_has_explicit_report_id(descriptor, descriptor_len);
    const uint8_t *parser_report = report;
    uint16_t parser_report_len = report_len;

    // hids_client prepends Report ID to notifications. If the descriptor has no
    // Report ID item, skip this synthetic byte to keep parser bit positions aligned.
    if (!descriptor_uses_report_id) {
        if (report_len <= 1u) {
            return;
        }
        parser_report = &report[1];
        parser_report_len = (uint16_t)(report_len - 1u);
    }

    btstack_hid_parser_t parser;
    btstack_hid_parser_init(&parser, descriptor, descriptor_len, HID_REPORT_TYPE_INPUT, parser_report, parser_report_len);

    s_hid_report_counter++;
    picokey_hid_log_report_brief(service_index, report_id, report, report_len);

    bool has_keyboard = false;
    uint8_t modifier = 0;
    uint8_t keycode[6] = {0};
    uint8_t keycode_count = 0;

    bool has_pointer_data = false;
    uint8_t buttons = 0;
    bool has_x = false;
    bool has_y = false;
    bool has_wheel = false;
    int32_t x_value = 0;
    int32_t y_value = 0;
    int32_t wheel_value = 0;
    bool x_relative = true;
    bool y_relative = true;
    bool wheel_relative = true;
    uint8_t active_contact_count = 0;
    bool has_contact_count_value = false;
    uint8_t reported_contact_count = 0;
    bool saw_tip_switch_42 = false;
    bool saw_tip_switch = false;
    bool selected_active_contact = false;
    bool accept_axes_for_current_contact = true;

    while (btstack_hid_parser_has_more(&parser)) {
        uint16_t usage_page;
        uint16_t usage;
        int32_t value;
        btstack_hid_parser_get_field(&parser, &usage_page, &usage, &value);

        uint8_t input_flags = (uint8_t)parser.descriptor_usage_item.descriptor_item.item_value;
        bool is_relative = (input_flags & 0x04u) != 0;

        if (usage_page == 0x07) {
            has_keyboard = true;

            if ((usage >= 0xE0u) && (usage <= 0xE7u)) {
                if (value != 0) {
                    modifier |= (uint8_t)(1u << (usage - 0xE0u));
                }
                continue;
            }

            if ((usage == 0) || (usage > 0xFFu) || (value == 0)) {
                continue;
            }

            if (keycode_count >= sizeof(keycode)) {
                continue;
            }

            uint8_t key = (uint8_t)usage;
            bool duplicate = false;
            for (uint8_t i = 0; i < keycode_count; i++) {
                if (keycode[i] == key) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                keycode[keycode_count++] = key;
            }
            continue;
        }

        if (usage_page == 0x09) {
            if ((usage >= 1u) && (usage <= 8u)) {
                has_pointer_data = true;
                if (value != 0) {
                    buttons |= (uint8_t)(1u << (usage - 1u));
                }
            }
            continue;
        }

        if (usage_page == 0x0D) {
            if (usage == 0x54u) {
                has_pointer_data = true;
                has_contact_count_value = true;
                if (value < 0) {
                    reported_contact_count = 0;
                } else if (value > 255) {
                    reported_contact_count = 255;
                } else {
                    reported_contact_count = (uint8_t)value;
                }
                continue;
            }

            // Tip Switch / In Range are used by many touchpads to mark active contact records.
            if ((usage == 0x42u) || (usage == 0x32u)) {
                if (usage == 0x42u) {
                    saw_tip_switch_42 = true;
                }
                if ((usage == 0x32u) && saw_tip_switch_42) {
                    continue;
                }

                has_pointer_data = true;
                saw_tip_switch = true;
                if ((value != 0) && (active_contact_count < 255u)) {
                    active_contact_count++;
                }
                if (!selected_active_contact && (value != 0)) {
                    selected_active_contact = true;
                    accept_axes_for_current_contact = true;
                } else {
                    accept_axes_for_current_contact = false;
                }
                continue;
            }
        }

        if ((usage_page == 0x01) || (usage_page == 0x0D)) {
            bool accept_axis = true;
            if (saw_tip_switch) {
                accept_axis = accept_axes_for_current_contact;
            }

            switch (usage) {
            case 0x30:
                if (!accept_axis) {
                    break;
                }
                has_pointer_data = true;
                has_x = true;
                x_value = value;
                x_relative = is_relative;
                break;
            case 0x31:
                if (!accept_axis) {
                    break;
                }
                has_pointer_data = true;
                has_y = true;
                y_value = value;
                y_relative = is_relative;
                break;
            case 0x38:
                if (!accept_axis) {
                    break;
                }
                has_pointer_data = true;
                has_wheel = true;
                wheel_value = value;
                wheel_relative = is_relative;
                break;
            default:
                break;
            }
            continue;
        }
    }

    if (has_contact_count_value) {
        active_contact_count = reported_contact_count;
    }

    if (has_keyboard) {
        picokey_usb_hid_send_keyboard_report(modifier, keycode);
    } else if (!has_pointer_data && (parser_report_len == 8u) && (parser_report[1] == 0u)) {
        // Fallback for devices that effectively send boot-keyboard-shaped reports
        // while the descriptor parse path does not expose keyboard usages.
        picokey_usb_hid_send_keyboard_report(parser_report[0], &parser_report[2]);
        picokey_pairing_monitor_log("[HID] boot-kbd fallback used (svc=%u id=%u)\r\n", service_index, report_id);
    }

    if (!has_pointer_data) {
        return;
    }

    int32_t dx = 0;
    int32_t dy = 0;
    int32_t wheel = 0;

    const bool touch_state_known = saw_tip_switch || has_contact_count_value;
    const bool touch_is_active = has_contact_count_value ? (active_contact_count > 0u) : selected_active_contact;
    uint8_t touch_contact_count = active_contact_count;
    if (touch_is_active && (touch_contact_count == 0u)) {
        touch_contact_count = 1u;
    }

    picokey_pointer_tracker_t *tracker = NULL;
    if ((has_x && !x_relative) || (has_y && !y_relative) || touch_state_known || (report_id == 6u)) {
        tracker = picokey_get_pointer_tracker(hids_cid, service_index, report_id, true);
    }

    bool touch_became_active = false;
    bool touch_became_inactive = false;
    bool suppress_first_touch_motion = false;
    if (tracker != NULL) {
        if (touch_state_known) {
            if (!touch_is_active) {
                touch_became_inactive = tracker->touch_active;
                // Finger/contact is up: clear absolute position baseline so the
                // next touch-down does not generate a jump.
                tracker->touch_active = false;
                tracker->have_x = false;
                tracker->have_y = false;
                tracker->scroll_remainder = 0;
            } else if (!tracker->touch_active) {
                tracker->touch_active = true;
                touch_became_active = true;
                suppress_first_touch_motion = true;
            }
        }
    }

    if (has_x) {
        if (x_relative) {
            dx = x_value;
        } else if (tracker != NULL) {
            if (suppress_first_touch_motion || !tracker->have_x) {
                dx = 0;
            } else {
                dx = x_value - tracker->last_x;
            }
            tracker->last_x = x_value;
            tracker->have_x = true;
        }
    }

    if (has_y) {
        if (y_relative) {
            dy = y_value;
        } else if (tracker != NULL) {
            if (suppress_first_touch_motion || !tracker->have_y) {
                dy = 0;
            } else {
                dy = y_value - tracker->last_y;
            }
            tracker->last_y = y_value;
            tracker->have_y = true;
        }
    }

    if (has_wheel) {
        // Most touchpads expose wheel as relative; absolute wheel is forwarded as-is.
        wheel = wheel_value;
        (void)wheel_relative;
    }

    if (report_id == 6u) {
        if (touch_contact_count < 2u) {
            dx = picokey_scale_touchpad_delta(dx);
            dy = picokey_scale_touchpad_delta(dy);
        }
    }

    if ((tracker != NULL) && (report_id == 6u)) {
        uint32_t now_ms = btstack_run_loop_get_time_ms();

        if (tracker->two_finger_join_pending) {
            bool join_valid = (int32_t)(tracker->two_finger_join_expire_ms - now_ms) >= 0;
            if (!join_valid) {
                tracker->two_finger_join_pending = false;
                tracker->two_finger_join_motion_accum = 0;
            }
        }

        if (touch_became_active) {
            tracker->drag_active = false;
            tracker->drag_arm_active_touch = false;
            tracker->tap_motion_accum = 0;
            tracker->two_finger_tap_motion_accum = 0;
            tracker->two_finger_join_motion_accum = 0;

            if (touch_contact_count == 1u) {
                bool drag_arm_valid = false;
                if (tracker->drag_arm_pending) {
                    drag_arm_valid = (int32_t)(tracker->drag_arm_expire_ms - now_ms) >= 0;
                }

                if (drag_arm_valid) {
                    tracker->drag_arm_pending = false;
                    tracker->drag_arm_active_touch = true;
                    tracker->tap_candidate = false;
                    tracker->two_finger_join_pending = false;
                } else {
                    tracker->drag_arm_pending = false;
                    tracker->tap_candidate = true;
                    tracker->tap_start_ms = now_ms;
                    tracker->two_finger_join_pending = true;
                    tracker->two_finger_join_expire_ms = now_ms + PICOKEY_TWO_FINGER_JOIN_MAX_DELAY_MS;
                }
                tracker->two_finger_tap_candidate = false;
            } else if (touch_contact_count == 2u) {
                tracker->drag_arm_pending = false;
                tracker->tap_candidate = false;
                tracker->two_finger_tap_candidate = true;
                tracker->two_finger_tap_start_ms = now_ms;
                tracker->two_finger_join_pending = false;
            } else {
                tracker->drag_arm_pending = false;
                tracker->tap_candidate = false;
                tracker->two_finger_tap_candidate = false;
                tracker->two_finger_join_pending = false;
            }
        }

        if (touch_is_active && !touch_became_active) {
            if (tracker->two_finger_join_pending && (touch_contact_count == 1u)) {
                tracker->two_finger_join_motion_accum += picokey_abs32(dx) + picokey_abs32(dy);
                if (tracker->two_finger_join_motion_accum > PICOKEY_TWO_FINGER_JOIN_MAX_TOTAL_MOTION) {
                    tracker->two_finger_join_pending = false;
                }
            }

            if ((tracker->last_touch_contact_count == 1u) && (touch_contact_count == 2u) &&
                tracker->two_finger_join_pending && (buttons == 0u)) {
                tracker->two_finger_join_pending = false;
                tracker->two_finger_tap_candidate = true;
                tracker->two_finger_tap_start_ms = now_ms;
                tracker->two_finger_tap_motion_accum = 0;
                tracker->tap_candidate = false;
                tracker->drag_arm_pending = false;
                tracker->tap_motion_accum = 0;
            }
        }

        if (touch_is_active && (touch_contact_count >= 2u)) {
            if (tracker->drag_active) {
                tracker->drag_active = false;
                picokey_usb_hid_send_mouse_report(0u, 0, 0, 0);
                picokey_pairing_monitor_log("[HID] drag-cancel (2-finger)\r\n");
            }
            tracker->drag_arm_pending = false;
            tracker->drag_arm_active_touch = false;
            tracker->two_finger_join_pending = false;
            tracker->tap_candidate = false;
            tracker->tap_motion_accum = 0;

            if ((touch_contact_count != 2u) || (buttons != 0u)) {
                tracker->two_finger_tap_candidate = false;
            }
            if (tracker->two_finger_tap_candidate) {
                tracker->two_finger_tap_motion_accum += picokey_abs32(dx) + picokey_abs32(dy);
                if (tracker->two_finger_tap_motion_accum > PICOKEY_TWO_FINGER_TAP_MAX_TOTAL_MOTION) {
                    tracker->two_finger_tap_candidate = false;
                }
            }

            // Invert direction to match expected touchpad scrolling behavior.
            tracker->scroll_remainder += dy;
            wheel += picokey_extract_scroll_steps(&tracker->scroll_remainder);
            dx = 0;
            dy = 0;
        } else {
            tracker->scroll_remainder = 0;
            if (touch_is_active && (touch_contact_count < 2u)) {
                if (tracker->last_touch_contact_count < 2u) {
                    tracker->two_finger_tap_candidate = false;
                    tracker->two_finger_tap_motion_accum = 0;
                }
            }
        }

        if (tracker->tap_candidate) {
            if (touch_became_inactive) {
                // Keep candidate until release handling below.
            } else if (!touch_is_active || (touch_contact_count != 1u) || (buttons != 0u)) {
                tracker->tap_candidate = false;
            } else {
                tracker->tap_motion_accum += picokey_abs32(dx) + picokey_abs32(dy);
                if (tracker->tap_motion_accum > PICOKEY_TAP_MAX_TOTAL_MOTION) {
                    tracker->tap_candidate = false;
                }
            }
        }

        if (tracker->drag_arm_active_touch) {
            if (!touch_is_active || (touch_contact_count != 1u) || (buttons != 0u)) {
                tracker->drag_arm_active_touch = false;
            } else {
                tracker->tap_motion_accum += picokey_abs32(dx) + picokey_abs32(dy);
                if ((tracker->tap_motion_accum > PICOKEY_TAP_MAX_TOTAL_MOTION) && !tracker->drag_active) {
                    tracker->drag_active = true;
                    picokey_pairing_monitor_log("[HID] drag-start\r\n");
                }
            }
        }

        if (touch_became_inactive) {
            bool emit_tap_click = false;
            bool emit_two_finger_tap_click = false;
            if (tracker->drag_active) {
                tracker->drag_active = false;
                picokey_usb_hid_send_mouse_report(0u, 0, 0, 0);
                picokey_pairing_monitor_log("[HID] drag-end\r\n");
            } else if (tracker->two_finger_tap_candidate) {
                uint32_t elapsed_ms = btstack_run_loop_get_time_ms() - tracker->two_finger_tap_start_ms;
                emit_two_finger_tap_click = elapsed_ms <= PICOKEY_TWO_FINGER_TAP_MAX_DURATION_MS;
            } else if (tracker->tap_candidate) {
                uint32_t elapsed_ms = btstack_run_loop_get_time_ms() - tracker->tap_start_ms;
                emit_tap_click = elapsed_ms <= PICOKEY_TAP_MAX_DURATION_MS;
            }

            tracker->drag_arm_active_touch = false;
            tracker->tap_candidate = false;
            tracker->tap_motion_accum = 0;
            tracker->two_finger_tap_candidate = false;
            tracker->two_finger_tap_motion_accum = 0;
            tracker->two_finger_join_pending = false;
            tracker->two_finger_join_motion_accum = 0;

            if (emit_two_finger_tap_click) {
                picokey_usb_hid_send_mouse_report(2u, 0, 0, 0);
                picokey_usb_hid_send_mouse_report(0u, 0, 0, 0);
                picokey_pairing_monitor_log("[HID] two-finger-tap-right-click\r\n");
                tracker->drag_arm_pending = false;
            } else if (emit_tap_click) {
                picokey_usb_hid_send_mouse_report(1u, 0, 0, 0);
                picokey_usb_hid_send_mouse_report(0u, 0, 0, 0);
                picokey_pairing_monitor_log("[HID] tap-click\r\n");
                tracker->drag_arm_pending = true;
                tracker->drag_arm_expire_ms = btstack_run_loop_get_time_ms() + PICOKEY_DRAG_ARM_TIMEOUT_MS;
            } else {
                tracker->drag_arm_pending = false;
            }
        }

        if (touch_is_active) {
            tracker->last_touch_contact_count = touch_contact_count;
        } else {
            tracker->last_touch_contact_count = 0;
        }

        if (tracker->drag_active) {
            buttons |= 1u;
        }
    }

    if ((buttons == 0u) && (dx == 0) && (dy == 0) && (wheel == 0)) {
        return;
    }

    if ((report_id == 6u) && ((s_hid_report_counter % 20u) == 0u)) {
        uint8_t drag_active = ((tracker != NULL) && tracker->drag_active) ? 1u : 0u;
        picokey_pairing_monitor_log("[HID] ptr id=%u c=%u btn=%u dr=%u has_xy=%u/%u dx=%ld dy=%ld wh=%ld\r\n",
                                   report_id, touch_contact_count, buttons, drag_active,
                                   has_x ? 1u : 0u, has_y ? 1u : 0u,
                                   (long)dx, (long)dy, (long)wheel);
    }

    picokey_usb_hid_send_mouse_report(buttons, picokey_clamp_i8(dx), picokey_clamp_i8(dy), picokey_clamp_i8(wheel));
}

static void picokey_hids_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    (void)channel;
    (void)size;

    bool is_gattservice_meta = (packet_type == HCI_EVENT_GATTSERVICE_META);

    if (packet_type == HCI_EVENT_PACKET) {
        uint8_t event_type = hci_event_packet_get_type(packet);

        if (event_type == HCI_EVENT_GATTSERVICE_META) {
            is_gattservice_meta = true;
        }

        if (event_type == HCI_EVENT_DISCONNECTION_COMPLETE) {
            picokey_reset_pointer_trackers();
            picokey_usb_hid_release_all();
        }
    }

    if (is_gattservice_meta) {
        switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
        case GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED:
            picokey_reset_pointer_trackers();
            s_hid_report_counter = 0;
            picokey_pairing_monitor_log("[HID] service connected (%u instances)\r\n",
                                       gattservice_subevent_hid_service_connected_get_num_instances(packet));
            break;
        case GATTSERVICE_SUBEVENT_HID_SERVICE_DISCONNECTED:
            picokey_reset_pointer_trackers();
            picokey_usb_hid_release_all();
            picokey_pairing_monitor_log("[HID] service disconnected\r\n");
            break;
        case GATTSERVICE_SUBEVENT_HID_REPORT:
            picokey_forward_hid_report(gattservice_subevent_hid_report_get_hids_cid(packet),
                                       gattservice_subevent_hid_report_get_service_index(packet),
                                       gattservice_subevent_hid_report_get_report_id(packet),
                                       gattservice_subevent_hid_report_get_report(packet),
                                       gattservice_subevent_hid_report_get_report_len(packet));
            break;
        default:
            break;
        }
    }

    if (s_app_hids_packet_handler != NULL) {
        s_app_hids_packet_handler(packet_type, channel, packet, size);
    }
}

uint8_t picokey_hids_client_connect(hci_con_handle_t con_handle, btstack_packet_handler_t packet_handler,
                                    hid_protocol_mode_t protocol_mode, uint16_t *hids_cid)
{
    s_app_hids_packet_handler = packet_handler;
    return hids_client_connect(con_handle, picokey_hids_packet_handler, protocol_mode, hids_cid);
}
