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

// Kontrol Volume & Mute (UAC1 / Windows 7+), index 0 = master
static int8_t mute[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1];
static int16_t volume_db[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1]; // dB bilangan bulat, 0 = 0 dB
static uint32_t current_sample_rate = AUDIO_SAMPLE_RATE;
static uint32_t last_packet_time_ms = 0;

static int32_t db_to_linear_gain(int16_t db) {
    if (db >= 0) return 256;
    int32_t gain = 256 + ((int32_t) db * 256) / 96;
    if (gain < 0) gain = 0;
    return gain;
}

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

bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void) rhport;
    uint8_t const itf = tu_u16_low(p_request->wIndex);
    uint8_t const alt = tu_u16_low(p_request->wValue);
    if (itf == ITF_NUM_AUDIO_STREAMING && alt != 0) {
        blink_interval_ms = BLINK_STREAMING;
    }
    return true;
}

bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void) rhport;
    uint8_t const itf = tu_u16_low(p_request->wIndex);
    uint8_t const alt = tu_u16_low(p_request->wValue);
    if (itf == ITF_NUM_AUDIO_STREAMING && alt == 0) {
        blink_interval_ms = BLINK_MOUNTED;
    }
    return true;
}

/*------------- Callback Audio Class (UAC1 — Windows 7) -------------*/

void tud_audio_feedback_params_cb(uint8_t func_id, uint8_t alt_itf, audio_feedback_params_t* feedback_param) {
    (void)func_id;
    (void)alt_itf;
    feedback_param->method = AUDIO_FEEDBACK_METHOD_FIFO_COUNT;
    feedback_param->sample_freq = current_sample_rate;
}

// TinyUSB 0.21+: data iso masuk ke FIFO, lalu ISR ini dipanggil (bukan rx_done_post_read_cb).
bool tud_audio_rx_done_isr(uint8_t rhport, uint16_t n_bytes_received, uint8_t func_id, uint8_t ep_out, uint8_t cur_alt_setting) {
    (void) rhport;
    (void) n_bytes_received;
    (void) func_id;
    (void) ep_out;
    (void) cur_alt_setting;
    return true;
}

static void process_usb_audio_samples(const int16_t *samples, uint16_t sample_frames) {
    for (uint16_t i = 0; i < sample_frames; i++) {
        int16_t l = samples[i * 2 + 0];
        int16_t r = samples[i * 2 + 1];

        if (mute[0] || mute[1] || mute[2]) {
            l = 0;
            r = 0;
        } else {
            int32_t g0 = db_to_linear_gain(volume_db[0]);
            int32_t g1 = db_to_linear_gain(volume_db[1]);
            int32_t g2 = db_to_linear_gain(volume_db[2]);
            l = (int16_t)((l * g0 * g1) >> 16);
            r = (int16_t)((r * g0 * g2) >> 16);
        }

        ring_buffer_push(&audio_rb, l, r);
    }
}

// Tarik audio dari FIFO TinyUSB (dipanggil dari main loop, seperti contoh uac2_speaker_fb).
static void usb_audio_task(void) {
    int16_t packet_buffer[CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX / 2];

    while (tud_audio_available() > 0) {
        uint16_t to_read = tud_audio_available();
        if (to_read > sizeof(packet_buffer)) {
            to_read = sizeof(packet_buffer);
        }
        uint16_t bytes_read = tud_audio_read(packet_buffer, to_read);
        if (bytes_read == 0) {
            break;
        }

        last_packet_time_ms = to_ms_since_boot(get_absolute_time());
        process_usb_audio_samples(packet_buffer, bytes_read / 4);
    }
}

static bool uac1_set_req_ep(tusb_control_request_t const *p_request, uint8_t *p_buff) {
    uint8_t ctrl_sel = TU_U16_HIGH(p_request->wValue);

    if (ctrl_sel == AUDIO10_EP_CTRL_SAMPLING_FREQ && p_request->bRequest == AUDIO10_CS_REQ_SET_CUR) {
        TU_VERIFY(p_request->wLength == 3);
        current_sample_rate = tu_unaligned_read32(p_buff) & 0x00FFFFFFu;
        return true;
    }
    return false;
}

static bool uac1_get_req_ep(uint8_t rhport, tusb_control_request_t const *p_request) {
    uint8_t ctrl_sel = TU_U16_HIGH(p_request->wValue);

    if (ctrl_sel == AUDIO10_EP_CTRL_SAMPLING_FREQ && p_request->bRequest == AUDIO10_CS_REQ_GET_CUR) {
        uint8_t freq[3] = {
            (uint8_t)(current_sample_rate & 0xFFu),
            (uint8_t)((current_sample_rate >> 8) & 0xFFu),
            (uint8_t)((current_sample_rate >> 16) & 0xFFu),
        };
        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, freq, sizeof(freq));
    }
    return false;
}

static bool uac1_set_req_entity(tusb_control_request_t const *p_request, uint8_t *p_buff) {
    uint8_t channel = TU_U16_LOW(p_request->wValue);
    uint8_t ctrl_sel = TU_U16_HIGH(p_request->wValue);
    uint8_t entity_id = TU_U16_HIGH(p_request->wIndex);

    if (entity_id != UAC1_ENTITY_FEATURE_UNIT || channel > CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX) {
        return false;
    }

    if (ctrl_sel == AUDIO10_FU_CTRL_MUTE && p_request->bRequest == AUDIO10_CS_REQ_SET_CUR) {
        TU_VERIFY(p_request->wLength == 1);
        mute[channel] = p_buff[0];
        return true;
    }
    if (ctrl_sel == AUDIO10_FU_CTRL_VOLUME && p_request->bRequest == AUDIO10_CS_REQ_SET_CUR) {
        TU_VERIFY(p_request->wLength == 2);
        volume_db[channel] = (int16_t)(tu_unaligned_read16(p_buff) / 256);
        return true;
    }
    return false;
}

static bool uac1_get_req_entity(uint8_t rhport, tusb_control_request_t const *p_request) {
    uint8_t channel = TU_U16_LOW(p_request->wValue);
    uint8_t ctrl_sel = TU_U16_HIGH(p_request->wValue);
    uint8_t entity_id = TU_U16_HIGH(p_request->wIndex);

    if (entity_id != UAC1_ENTITY_FEATURE_UNIT || channel > CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX) {
        return false;
    }

    if (ctrl_sel == AUDIO10_FU_CTRL_MUTE) {
        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &mute[channel], 1);
    }

    if (ctrl_sel == AUDIO10_FU_CTRL_VOLUME) {
        switch (p_request->bRequest) {
            case AUDIO10_CS_REQ_GET_CUR: {
                int16_t vol = (int16_t)(volume_db[channel] * 256);
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &vol, sizeof(vol));
            }
            case AUDIO10_CS_REQ_GET_MIN: {
                int16_t min = (int16_t)(-90 * 256);
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &min, sizeof(min));
            }
            case AUDIO10_CS_REQ_GET_MAX: {
                int16_t max = 0;
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &max, sizeof(max));
            }
            case AUDIO10_CS_REQ_GET_RES: {
                int16_t res = 256;
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &res, sizeof(res));
            }
            default:
                break;
        }
    }
    return false;
}

bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *p_buff) {
    (void) rhport;
    return uac1_set_req_ep(p_request, p_buff);
}

bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    return uac1_get_req_ep(rhport, p_request);
}

bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *buf) {
    (void) rhport;
    return uac1_set_req_entity(p_request, buf);
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    return uac1_get_req_entity(rhport, p_request);
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
        tud_task();
        usb_audio_task();

        // Alirkan sampel audio dari ring buffer ke hardware I2S PIO PCM5102A
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
