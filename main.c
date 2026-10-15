/**
 * Raspberry Pi Pico - PCM5102A USB Audio Sound Card (USB DAC)
 * Mengubah Raspberry Pi Pico menjadi USB DAC eksternal untuk Komputer/Laptop.
 *
 * Pengkabelan (Wiring):
 *  - Pico GP16 (Pin 21) -> PCM5102 BCK (Bit Clock)
 *  - Pico GP17 (Pin 22) -> PCM5102 LCK (Word Clock / LRCK)
 *  - Pico GP18 (Pin 24) -> PCM5102 DIN (Data In)
 *  - Pico 3V3 (Pin 36)  -> PCM5102 VCC & XMT (Un-mute)
 *  - Pico GND (Pin 38)  -> PCM5102 GND, SCK (PLL Internal), FMT (I2S Format)
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "audio_i2s.h"
#include "ring_buffer.h"

#define AUDIO_SAMPLE_RATE 48000

// Status indikator LED
enum {
    BLINK_STREAMING   = 20,    // LED menyala terus saat audio streaming
    BLINK_NOT_MOUNTED = 250,   // Kedip cepat jika belum terhubung ke PC
    BLINK_MOUNTED     = 1000,  // Kedip santai jika terhubung & standby
    BLINK_SUSPENDED   = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
static i2s_config_t i2s;
static audio_ring_buffer_t audio_rb;

// Kontrol Volume & Mute dari Host OS (Windows/Mac/Linux)
static bool is_muted = false;
static int16_t volume_master = 256; // 256 = 100% volume (0 dB)
static uint32_t last_packet_time_ms = 0;

/*------------- Inisialisasi LED -------------*/
void led_init(void) {
#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
#endif
}

void led_task(void) {
#ifdef PICO_DEFAULT_LED_PIN
    static uint32_t start_ms = 0;
    static bool led_state = false;
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // Jika audio sedang streaming aktif (ada paket dalam 100ms terakhir), nyalakan LED penuh
    if (now - last_packet_time_ms < 100) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        return;
    }

    if (now - start_ms < blink_interval_ms) return;
    start_ms = now;

    led_state = !led_state;
    gpio_put(PICO_DEFAULT_LED_PIN, led_state);
#endif
}

/*------------- Callback Status Koneksi USB Device -------------*/
void tud_mount_cb(void) {
    blink_interval_ms = BLINK_MOUNTED;
}

void tud_umount_cb(void) {
    blink_interval_ms = BLINK_NOT_MOUNTED;
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    blink_interval_ms = BLINK_SUSPENDED;
}

void tud_resume_cb(void) {
    blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

/*------------- Callback Audio Class (UAC2) -------------*/

// Asynchronous Feedback: Memberi tahu host PC kecepatan konsumsi sampel audio
void tud_audio_feedback_params_cb(uint8_t func_id, uint8_t alt_itf, audio_feedback_params_t* feedback_param) {
    (void)func_id;
    (void)alt_itf;
    feedback_param->method = AUDIO_FEEDBACK_METHOD_FIFO_COUNT;
    feedback_param->sample_freq = AUDIO_SAMPLE_RATE;
}

// Callback saat paket audio diterima dari Komputer via USB
// Gunakan post_read: pada titik ini data paket sudah dimasukkan TinyUSB ke FIFO.
bool tud_audio_rx_done_post_read_cb(uint8_t rhport, uint16_t n_bytes_received, uint8_t func_id, uint8_t ep_out, uint8_t cur_alt_setting) {
    (void)rhport;
    (void)func_id;
    (void)ep_out;
    (void)cur_alt_setting;

    last_packet_time_ms = to_ms_since_boot(get_absolute_time());

    int16_t packet_buffer[CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX / 2];
    // Batasi agar tidak melebihi ukuran buffer (mencegah stack overflow)
    uint16_t to_read = n_bytes_received;
    if (to_read > sizeof(packet_buffer)) to_read = sizeof(packet_buffer);
    uint16_t bytes_read = tud_audio_read(packet_buffer, to_read);
    uint16_t sample_frames = bytes_read / 4; // 2 byte Left, 2 byte Right

    for (uint16_t i = 0; i < sample_frames; i++) {
        int16_t l = packet_buffer[i * 2 + 0];
        int16_t r = packet_buffer[i * 2 + 1];

        // Terapkan Mute & Volume
        if (is_muted) {
            l = 0;
            r = 0;
        } else if (volume_master < 256) {
            l = (int16_t)((l * volume_master) >> 8);
            r = (int16_t)((r * volume_master) >> 8);
        }

        ring_buffer_push(&audio_rb, l, r);
    }

    return true;
}

// Kontrol Clock USB Audio
bool tud_audio_clock_get_request(uint8_t rhport, audio_control_request_t const *request) {
    if (request->bEntityID == UAC2_ENTITY_CLOCK && request->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ) {
        if (request->bRequest == AUDIO_CS_REQ_CUR) {
            audio_control_cur_4_t curf = { (int32_t)tu_htole32(AUDIO_SAMPLE_RATE) };
            return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &curf, sizeof(curf));
        } else if (request->bRequest == AUDIO_CS_REQ_RANGE) {
            audio_control_range_4_n_t(1) rangef = {
                .wNumSubRanges = tu_htole16(1),
                .subrange[0] = {
                    .bMin = (int32_t)AUDIO_SAMPLE_RATE,
                    .bMax = (int32_t)AUDIO_SAMPLE_RATE,
                    .bRes = 0
                }
            };
            return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &rangef, sizeof(rangef));
        }
    } else if (request->bEntityID == UAC2_ENTITY_CLOCK && request->bControlSelector == AUDIO_CS_CTRL_CLK_VALID) {
        audio_control_cur_1_t cur_valid = { .bCur = 1 };
        return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &cur_valid, sizeof(cur_valid));
    }
    return false;
}

bool tud_audio_clock_set_request(uint8_t rhport, audio_control_request_t const *request, uint8_t const *buf) {
    (void)rhport;
    (void)buf;
    if (request->bEntityID == UAC2_ENTITY_CLOCK && request->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ) {
        return true;
    }
    return false;
}

// Kontrol Feature Unit: Volume & Mute dari Sistem Operasi
bool tud_audio_feature_unit_get_request(uint8_t rhport, audio_control_request_t const *request) {
    if (request->bEntityID != UAC2_ENTITY_FEATURE_UNIT) return false;

    if (request->bControlSelector == AUDIO_FU_CTRL_MUTE && request->bRequest == AUDIO_CS_REQ_CUR) {
        audio_control_cur_1_t cur_mute = { .bCur = is_muted ? 1 : 0 };
        return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &cur_mute, sizeof(cur_mute));
    } else if (request->bControlSelector == AUDIO_FU_CTRL_VOLUME) {
        if (request->bRequest == AUDIO_CS_REQ_RANGE) {
            audio_control_range_2_n_t(1) range_vol = {
                .wNumSubRanges = tu_htole16(1),
                .subrange[0] = {
                    .bMin = tu_htole16((int16_t)(-96 * 256)), // -96 dB
                    .bMax = tu_htole16(0),                   // 0 dB
                    .bRes = tu_htole16(256)                  // Step 1 dB
                }
            };
            return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &range_vol, sizeof(range_vol));
        } else if (request->bRequest == AUDIO_CS_REQ_CUR) {
            int16_t vol_db = (volume_master >= 256) ? 0 : (int16_t)((volume_master - 256) * 96);
            audio_control_cur_2_t cur_vol = { .bCur = tu_htole16(vol_db) };
            return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &cur_vol, sizeof(cur_vol));
        }
    }
    return false;
}

bool tud_audio_feature_unit_set_request(uint8_t rhport, audio_control_request_t const *request, uint8_t const *buf) {
    (void)rhport;
    if (request->bEntityID != UAC2_ENTITY_FEATURE_UNIT) return false;

    if (request->bControlSelector == AUDIO_FU_CTRL_MUTE && request->bRequest == AUDIO_CS_REQ_CUR) {
        is_muted = ((audio_control_cur_1_t const *)buf)->bCur != 0;
        return true;
    } else if (request->bControlSelector == AUDIO_FU_CTRL_VOLUME && request->bRequest == AUDIO_CS_REQ_CUR) {
        int16_t vol_db = (int16_t)tu_le16toh(((audio_control_cur_2_t const *)buf)->bCur);
        if (vol_db >= 0) {
            volume_master = 256;
        } else if (vol_db <= -96 * 256) {
            volume_master = 0;
        } else {
            // Pemetaan linear sederhana
            volume_master = (int16_t)(256 + (vol_db / 96));
            if (volume_master < 0) volume_master = 0;
        }
        return true;
    }
    return false;
}

// Callback resmi TinyUSB untuk request class-specific ke Entity (Clock / Feature Unit).
// Tanpa ini, TinyUSB memakai versi weak yang selalu STALL sehingga host
// (Windows/Android) gagal menjalankan device ("device cannot start").
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    audio_control_request_t const *request = (audio_control_request_t const *)p_request;

    if (request->bEntityID == UAC2_ENTITY_CLOCK) {
        return tud_audio_clock_get_request(rhport, request);
    }
    if (request->bEntityID == UAC2_ENTITY_FEATURE_UNIT) {
        return tud_audio_feature_unit_get_request(rhport, request);
    }
    return false;
}

bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *buf) {
    audio_control_request_t const *request = (audio_control_request_t const *)p_request;

    if (request->bEntityID == UAC2_ENTITY_CLOCK) {
        return tud_audio_clock_set_request(rhport, request, buf);
    }
    if (request->bEntityID == UAC2_ENTITY_FEATURE_UNIT) {
        return tud_audio_feature_unit_set_request(rhport, request, buf);
    }
    return false;
}

/*------------- Program Utama (MAIN) -------------*/
int main(void) {
    // Inisialisasi clock sistem & LED
    stdio_init_all();
    led_init();

    // Inisialisasi Ring Buffer Audio
    ring_buffer_init(&audio_rb);

    // Inisialisasi Driver I2S PIO untuk modul PCM5102A
    // GP16 = BCLK, GP17 = LCK (LRCK), GP18 = DIN
    if (!audio_i2s_init(&i2s, pio0, 0, I2S_PIN_BCLK, I2S_PIN_DIN, AUDIO_SAMPLE_RATE)) {
        while (1) {
            tight_loop_contents();
        }
    }

    // Inisialisasi TinyUSB Device Stack
    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO
    };
    tusb_init(0, &dev_init);


    while (1) {
        // 1. Jalankan proses USB Stack TinyUSB
        tud_task();

        // 2. Alirkan sampel audio dari ring buffer ke hardware I2S PIO PCM5102A
        while (audio_i2s_can_put_sample(&i2s)) {
            int16_t left = 0, right = 0;
            // Jika ada data audio dari USB, kirimkan.
            // Jika buffer kosong (misal PC sedang hening/pause), kirim 0 agar osilator PLL PCM5102 tetap sinkron.
            ring_buffer_pop(&audio_rb, &left, &right);
            audio_i2s_put_sample(&i2s, left, right);
        }

        // 3. Update status kedipan LED indikator
        led_task();
    }

    return 0;
}
