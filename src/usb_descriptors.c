#include <string.h>

#include "picokey_usb_reports.h"
#include "tusb.h"

#define USB_VID 0x2E8A
#define USB_PID 0x00C1
#define USB_BCD 0x0200

static const tusb_desc_device_t desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = USB_BCD,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

static const uint8_t desc_hid_report_keyboard[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_HID_KEYBOARD,
    ITF_NUM_TOTAL,
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN)

#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT 0x02
#define EPNUM_CDC_IN 0x82
#define EPNUM_HID_KEYBOARD 0x83

static const uint8_t desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN,
                       CFG_TUD_CDC_EP_BUFSIZE),

    TUD_HID_DESCRIPTOR(ITF_NUM_HID_KEYBOARD, 0, HID_ITF_PROTOCOL_KEYBOARD, sizeof(desc_hid_report_keyboard),
                       EPNUM_HID_KEYBOARD, CFG_TUD_HID_EP_BUFSIZE, 5),
};

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_CDC,
};

static const char *const string_desc_arr[] = {
    (const char[]){0x09, 0x04},
    "picokey",
    "PicoKey BLE Bridge",
    "0001",
    "PicoKey Pairing Log",
};

static uint16_t desc_str[32 + 1];

const uint8_t *tud_descriptor_device_cb(void)
{
    return (const uint8_t *)&desc_device;
}

const uint8_t *tud_hid_descriptor_report_cb(uint8_t instance)
{
    if (instance == PICOKEY_USB_HID_ITF_KEYBOARD) {
        return desc_hid_report_keyboard;
    }

    return NULL;
}

const uint8_t *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;

    size_t chr_count;
    if (index == STRID_LANGID) {
        memcpy(&desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= (sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
            return NULL;
        }
        const char *str = string_desc_arr[index];
        chr_count = strlen(str);
        if (chr_count > 32) {
            chr_count = 32;
        }
        for (size_t i = 0; i < chr_count; i++) {
            desc_str[1 + i] = str[i];
        }
    }

    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return desc_str;
}

void tud_cdc_rx_cb(uint8_t itf)
{
    (void)itf;
    while (tud_cdc_available()) {
        (void)tud_cdc_read_char();
    }
}

