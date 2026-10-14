"""
I2S Audio DAC Controller untuk Raspberry Pi Pico
Mengelola inisialisasi peripheral machine.I2S, kontrol volume, dan transmisi data audio.
"""

import struct
try:
    from machine import I2S, Pin
    MICROPYTHON_ENV = True
except ImportError:
    MICROPYTHON_ENV = False

import config

class I2SDAC:
    def __init__(self,
                 i2s_id=config.I2S_ID,
                 sck_pin=config.PIN_SCK,
                 ws_pin=config.PIN_WS,
                 sd_pin=config.PIN_SD,
                 sample_rate=config.DEFAULT_SAMPLE_RATE,
                 bit_depth=config.DEFAULT_BIT_DEPTH,
                 stereo=True,
                 buffer_size=config.BUFFER_SIZE):
        """
        Inisialisasi antarmuka I2S DAC pada Raspberry Pi Pico RP2040.
        Perhatikan: ws_pin harus bernilai sck_pin + 1 karena batasan PIO RP2040.
        """
        if ws_pin != sck_pin + 1:
            raise ValueError(f"Batasan RP2040: ws_pin ({ws_pin}) harus bernilai sck_pin + 1 ({sck_pin + 1})")

        self.i2s_id = i2s_id
        self.sck_pin = sck_pin
        self.ws_pin = ws_pin
        self.sd_pin = sd_pin
        self.sample_rate = sample_rate
        self.bit_depth = bit_depth
        self.stereo = stereo
        self.buffer_size = buffer_size
        self.volume = config.DEFAULT_VOLUME
        self.i2s = None

        if MICROPYTHON_ENV:
            self._init_i2s()

    def _init_i2s(self):
        """Inisialisasi objek machine.I2S di MicroPython"""
        if self.i2s is not None:
            try:
                self.i2s.deinit()
            except Exception:
                pass

        audio_format = I2S.STEREO if self.stereo else I2S.MONO

        self.i2s = I2S(
            self.i2s_id,
            sck=Pin(self.sck_pin),
            ws=Pin(self.ws_pin),
            sd=Pin(self.sd_pin),
            mode=I2S.TX,
            bits=self.bit_depth,
            format=audio_format,
            rate=self.sample_rate,
            ibuf=self.buffer_size
        )

    def reconfigure(self, sample_rate=None, stereo=None, bit_depth=None):
        """Mengubah sample rate atau format audio bila diperlukan (misal saat membaca file WAV)"""
        changed = False
        if sample_rate is not None and sample_rate != self.sample_rate:
            self.sample_rate = sample_rate
            changed = True
        if stereo is not None and stereo != self.stereo:
            self.stereo = stereo
            changed = True
        if bit_depth is not None and bit_depth != self.bit_depth:
            self.bit_depth = bit_depth
            changed = True

        if changed and MICROPYTHON_ENV:
            self._init_i2s()

    def set_volume(self, volume: float):
        """Mengatur skala volume dari 0.0 (hening) sampai 1.0 (maksimum)"""
        self.volume = max(0.0, min(1.0, float(volume)))

    def get_volume(self) -> float:
        return self.volume

    def write(self, buf):
        """Mengirim buffer byte PCM ke modul DAC I2S"""
        if not MICROPYTHON_ENV or self.i2s is None:
            # Mode simulasi di host PC
            return len(buf)
        return self.i2s.write(buf)

    def write_with_volume(self, buf: bytearray or bytes):
        """Mengirim buffer dengan menerapkan faktor volume secara real-time"""
        if self.volume >= 0.99:
            return self.write(buf)
        if self.volume <= 0.01:
            # Hening (0)
            silent = bytearray(len(buf))
            return self.write(silent)

        # Skalakan sampel 16-bit bertanda (signed 16-bit little-endian)
        num_samples = len(buf) // 2
        fmt = f"<{num_samples}h"
        samples = struct.unpack(fmt, buf)
        vol = self.volume
        scaled_samples = [int(s * vol) for s in samples]
        scaled_buf = struct.pack(fmt, *scaled_samples)
        return self.write(scaled_buf)

    def deinit(self):
        """Melepas sumber daya hardware I2S"""
        if MICROPYTHON_ENV and self.i2s is not None:
            try:
                self.i2s.deinit()
            except Exception:
                pass
            self.i2s = None
