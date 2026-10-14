"""
Konfigurasi Pin & Audio Parameter untuk Raspberry Pi Pico I2S DAC
Kompatibel dengan modul MAX98357A, PCM5102A, UDA1334A, dll.
"""

# ==============================================================================
# PENTING UNTUK RASPBERRY PI PICO (RP2040):
# Driver PIO I2S pada MicroPython mewajibkan pin Word Select (WS/LRCK)
# harus tepat satu nomor di atas pin Serial Clock (SCK/BCLK):
# WS = SCK + 1
# ==============================================================================

# I2S Bus ID (0 atau 1 pada RP2040)
I2S_ID = 0

# Pin GPIO Raspberry Pi Pico
PIN_SCK = 16  # Bit Clock (BCLK / BCK / SCK)    -> GP16 (Physical Pin 21)
PIN_WS = 17   # Word Select (LRCK / LCK / WS)     -> GP17 (Physical Pin 22)
PIN_SD = 18   # Serial Data (DIN / DATA / SD)    -> GP18 (Physical Pin 24)

# Konfigurasi Audio Default
DEFAULT_SAMPLE_RATE = 22050  # Hertz (Umum: 16000, 22050, 44100, 48000)
DEFAULT_BIT_DEPTH = 16       # 16-bit signed PCM
BUFFER_SIZE = 2048           # Ukuran buffer internal I2S (byte)
DEFAULT_VOLUME = 0.7         # Skala volume master (0.0 sampai 1.0)
