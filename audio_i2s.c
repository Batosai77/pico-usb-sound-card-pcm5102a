#include "audio_i2s.h"
#include "audio_i2s.pio.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"

bool audio_i2s_init(i2s_config_t *config, PIO pio, uint sm, uint bclk_pin, uint din_pin, uint32_t sample_rate) {
    config->pio = pio;
    config->sm = sm;
    config->bclk_pin = bclk_pin;
    config->lrck_pin = bclk_pin + 1; // Pin LRCK wajib tepat di atas BCLK (GP16 & GP17)
    config->din_pin = din_pin;
    config->sample_rate = sample_rate;

    if (!pio_can_add_program(pio, &audio_i2s_program)) {
        return false;
    }

    config->offset = pio_add_program(pio, &audio_i2s_program);

    pio_gpio_init(pio, bclk_pin);
    pio_gpio_init(pio, bclk_pin + 1);
    pio_gpio_init(pio, din_pin);

    audio_i2s_program_init(pio, sm, config->offset, din_pin, bclk_pin);
    audio_i2s_set_sample_rate(config, sample_rate);

    pio_sm_set_enabled(pio, sm, true);
    return true;
}

void audio_i2s_set_sample_rate(i2s_config_t *config, uint32_t sample_rate) {
    config->sample_rate = sample_rate;
    // 1 frame stereo 16-bit = 32 bit data = 64 siklus PIO (2 siklus per bit)
    uint32_t system_clock_hz = clock_get_hz(clk_sys);
    float divider = (float)system_clock_hz / (float)(sample_rate * 64);
    pio_sm_set_clkdiv(config->pio, config->sm, divider);
}

void audio_i2s_deinit(i2s_config_t *config) {
    pio_sm_set_enabled(config->pio, config->sm, false);
    pio_remove_program(config->pio, &audio_i2s_program, config->offset);
}
