#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_RP2040
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS  OPT_OS_NONE
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG 0
#endif

#define CFG_TUD_ENABLED       1
#define CFG_TUD_MAX_SPEED     OPT_MODE_DEFAULT_SPEED
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN    __attribute__ ((aligned(4)))
#endif

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE 64
#endif

#define CFG_TUD_AUDIO         1
#define CFG_TUD_CDC           0
#define CFG_TUD_MSC           0
#define CFG_TUD_HID           0
#define CFG_TUD_MIDI          0
#define CFG_TUD_VENDOR        0

#include "usb_descriptors.h"

#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX               2
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX       2
#define CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX               16

#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_FS          48000
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_FS                TUD_AUDIO_EP_SIZE(false, CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_FS, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX)
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX               CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_FS
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ            (4 * CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_FS)

#define CFG_TUD_AUDIO_ENABLE_EP_OUT                      1
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP                 1

#ifdef __cplusplus
}
#endif

#endif
