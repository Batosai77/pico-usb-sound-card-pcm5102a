#ifndef AUDIO_I2S_H
#define AUDIO_I2S_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/pio.h"

#ifdef __cplusplus
extern "C" {
#endif

// Konfigurasi pin I2S default
#define I2S_PIN_BCLK 16   // Bit Clock (GP16)
#define I2S_PIN_LRCK 17   // Left/Right Word Clock (GP17)
#define I2S_PIN_DIN  18   // Data In / Serial Data (GP18)

typedef struct {
    PIO pio;
    uint sm;
    uint offset;
    uint bclk_pin;
    uint lrck_pin;
    uint din_pin;
    uint32_t sample_rate;
} i2s_config_t;

/**
 * Inisialisasi antarmuka I2S pada PIO
 */
bool audio_i2s_init(i2s_config_t *config, PIO pio, uint sm, uint bclk_pin, uint din_pin, uint32_t sample_rate);

/**
 * Mengatur frekuensi sample rate (misal 22050, 44100, 48000 Hz)
 */
void audio_i2s_set_sample_rate(i2s_config_t *config, uint32_t sample_rate);

/**
 * Mengirim satu frame sampel stereo 16-bit (Kiri & Kanan)
 */
static inline void audio_i2s_put_sample(i2s_config_t *config, int16_t left, int16_t right) {
    // Format FIFO 32-bit: Left di MSB (16 bit atas), Right di LSB (16 bit bawah)
    uint32_t word = ((uint32_t)(uint16_t)left << 16) | (uint32_t)(uint16_t)right;
    pio_sm_put_blocking(config->pio, config->sm, word);
}

static inline bool audio_i2s_can_put_sample(i2s_config_t *config) {
    return !pio_sm_is_tx_fifo_full(config->pio, config->sm);
}

static inline bool audio_i2s_put_sample_nonblocking(i2s_config_t *config, int16_t left, int16_t right) {
    if (pio_sm_is_tx_fifo_full(config->pio, config->sm)) {
        return false;
    }
    uint32_t word = ((uint32_t)(uint16_t)left << 16) | (uint32_t)(uint16_t)right;
    pio_sm_put(config->pio, config->sm, word);
    return true;
}


/**
 * Menonaktifkan state machine I2S
 */
void audio_i2s_deinit(i2s_config_t *config);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_I2S_H
