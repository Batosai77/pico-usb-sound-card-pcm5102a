"""
Program Utama - Raspberry Pi Pico I2S Audio DAC
Menjalankan demonstrasi audio:
 1. Tangga nada musik (Musical scale: Do-Re-Mi)
 2. Perbandingan bentuk gelombang (Sine, Triangle, Sawtooth, Square)
 3. Efek suara (Chirp sweep, Coin SFX)
 4. Pemutaran file audio WAV (Streaming 16-bit PCM)
"""

import time
import os
import config
from i2s_audio import I2SDAC
import tone_generator
import wav_player

# Kompatibilitas helper sleep
if not hasattr(time, 'sleep_ms'):
    time.sleep_ms = lambda ms: time.sleep(ms / 1000.0)

def print_banner():
    print("=" * 60)
    print("      RASPBERRY PI PICO I2S AUDIO DAC CONTROLLER")
    print("=" * 60)
    print(f"  • Bit Clock (SCK / BCLK) : GP{config.PIN_SCK} (Pin Fisik 21)")
    print(f"  • Word Select (WS / LRC) : GP{config.PIN_WS} (Pin Fisik 22)  [Wajib SCK+1]")
    print(f"  • Serial Data (SD / DIN) : GP{config.PIN_SD} (Pin Fisik 24)")
    print(f"  • Default Sample Rate    : {config.DEFAULT_SAMPLE_RATE} Hz")
    print(f"  • Volume Master          : {int(config.DEFAULT_VOLUME * 100)}%")
    print("=" * 60)

def demo_musical_scale(dac):
    print("\n[1/4] Memainkan Tangga Nada Musik (Do-Re-Mi-Fa-Sol-La-Ti-Do)...")
    tone_generator.play_melody(dac, tone_generator.MELODY_SCALE, wave_type='sine')
    time.sleep_ms(400)

def demo_waveforms(dac):
    print("\n[2/4] Menguji Karakter Suara 4 Bentuk Gelombang (440 Hz - Nada A4)...")
    waves = [
        ('sine', 'Gelombang Sinus (Murni & Lembut)'),
        ('triangle', 'Gelombang Segitiga (Hangat)'),
        ('sawtooth', 'Gelombang Gergaji (Kaya Harmonik)'),
        ('square', 'Gelombang Kotak (Khas 8-bit Retro Chiptune)')
    ]
    for w_type, desc in waves:
        print(f"  -> {desc}...")
        tone_generator.play_tone(dac, 440, 600, wave_type=w_type)
        time.sleep_ms(250)

def demo_sound_effects(dac):
    print("\n[3/4] Memainkan Efek Suara (Sound Effects)...")
    print("  -> Efek Nada Koin (Coin SFX)...")
    tone_generator.play_melody(dac, tone_generator.MELODY_COIN, wave_type='square')
    time.sleep_ms(300)

    print("  -> Nada Meluncur Naik (Frequency Sweep Up)...")
    tone_generator.play_sweep(dac, start_freq=300, end_freq=1800, duration_ms=400, steps=25)
    time.sleep_ms(200)

    print("  -> Nada Meluncur Turun (Frequency Sweep Down)...")
    tone_generator.play_sweep(dac, start_freq=1800, end_freq=200, duration_ms=400, steps=25)
    time.sleep_ms(300)

    print("  -> Melodi Jingle Chime...")
    tone_generator.play_melody(dac, tone_generator.MELODY_CHIME, wave_type='sine')
    time.sleep_ms(400)

def demo_wav_playback(dac):
    print("\n[4/4] Menguji Pemutaran File WAV Streaming...")
    wav_filename = "sample.wav"
    try:
        # Cek ketersediaan file
        os.stat(wav_filename)
        wav_player.play_wav(dac, wav_filename)
    except OSError:
        print(f"  [Peringatan] File '{wav_filename}' tidak ditemukan di flash Raspberry Pi Pico.")
        print("  Jalankan 'generate_sample_wav.py' terlebih dahulu untuk membuat file audio.")

def main():
    print_banner()

    # Inisialisasi DAC I2S
    print("\nMenginisialisasi peripheral I2S DAC...")
    dac = I2SDAC(
        sck_pin=config.PIN_SCK,
        ws_pin=config.PIN_WS,
        sd_pin=config.PIN_SD,
        sample_rate=config.DEFAULT_SAMPLE_RATE,
        stereo=True
    )
    dac.set_volume(config.DEFAULT_VOLUME)
    print("DAC siap digunakan.")

    try:
        # Jalankan demonstrasi lengkap
        demo_musical_scale(dac)
        demo_waveforms(dac)
        demo_sound_effects(dac)
        demo_wav_playback(dac)

        print("\n" + "=" * 60)
        print("Semua demonstrasi audio DAC selesai dijalankan dengan sukses!")
        print("=" * 60)

    except KeyboardInterrupt:
        print("\nProgram dihentikan oleh pengguna (Ctrl+C).")
    finally:
        # Selalu bersihkan resource hardware I2S
        dac.deinit()
        print("Hardware I2S dinonaktifkan dengan aman.")

if __name__ == "__main__":
    main()
