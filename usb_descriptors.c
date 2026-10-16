#include "tusb.h"
#include "usb_descriptors.h"
#include "pico/unique_id.h"

#define USB_VID 0x2E8A  // Raspberry Pi Official Vendor ID
#define USB_PID 0x4010  // USB Audio Device Product ID

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0201,

    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0200,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_AUDIO_SPEAKER_STEREO_FB_DESC_LEN)

uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA (200mA)
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 200),

    // Audio Stereo Speaker with Feedback Descriptor
    TUD_AUDIO_SPEAKER_STEREO_FB_DESCRIPTOR(
        0,                                           // Interface number
        4,                                           // String index nama interface
        CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX,  // 2 bytes per sample (16-bit)
        CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX,          // 16-bit
        EPNUM_AUDIO_OUT,                             // EP OUT address (0x01)
        CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX,          // Max EP size (192 byte)
        EPNUM_AUDIO_FB | 0x80,                       // Feedback EP IN (0x81)
        4                                            // Feedback size (4 bytes)
    )
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+
static char const *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },    // 0: Bahasa (English US)
    "Raspberry Pi",                   // 1: Manufacturer
    "Pico PCM5102A USB DAC",          // 2: Product
    NULL,                             // 3: Serial number unik (diambil dari Pico)
    "PCM5102A Audio Output"           // 4: Audio Interface String
};

static uint16_t _desc_str[64];

uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else if (index == 3) {
        // Serial number unik board RP2040
        char pico_id_str[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
        pico_get_unique_board_id_string(pico_id_str, sizeof(pico_id_str));
        chr_count = (uint8_t)strlen(pico_id_str);
        if (chr_count > 31) chr_count = 31;
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = pico_id_str[i];
        }
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
            return NULL;
        }
        const char *str = string_desc_arr[index];
        if (str == NULL) return NULL;
        chr_count = (uint8_t)strlen(str);
        if (chr_count > 31) chr_count = 31;
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
    return _desc_str;
}
