"""
Tone & Waveform Generator untuk Raspberry Pi Pico I2S DAC
Mendukung pembangkitan gelombang sinus, kotak, segitiga, dan gergaji,
serta memainkan nada musik & melodi tanpa jeda (glitch-free).
"""

import math
import struct
import time

# Kompatibilitas dengan Python standar desktop jika diuji di luar MicroPython
if not hasattr(time, 'sleep_ms'):
    time.sleep_ms = lambda ms: time.sleep(ms / 1000.0)


# Frekuensi nada musik standar (Hz) - Oktaf 3 sampai 6
NOTES = {
    'REST': 0,
    # Oktaf 3
    'C3': 130.81, 'CS3': 138.59, 'D3': 146.83, 'DS3': 155.56, 'E3': 164.81,
    'F3': 174.61, 'FS3': 185.00, 'G3': 196.00, 'GS3': 207.65, 'A3': 220.00,
    'AS3': 233.08, 'B3': 246.94,
    # Oktaf 4 (Standard Tengah)
    'C4': 261.63, 'CS4': 277.18, 'D4': 293.66, 'DS4': 311.13, 'E4': 329.63,
    'F4': 349.23, 'FS4': 369.99, 'G4': 392.00, 'GS4': 415.30, 'A4': 440.00,
    'AS4': 466.16, 'B4': 493.88,
    # Oktaf 5
    'C5': 523.25, 'CS5': 554.37, 'D5': 587.33, 'DS5': 622.25, 'E5': 659.25,
    'F5': 698.46, 'FS5': 739.99, 'G5': 783.99, 'GS5': 830.61, 'A5': 880.00,
    'AS5': 932.33, 'B5': 987.77,
    # Oktaf 6
    'C6': 1046.50
}

def generate_tone_buffer(freq, sample_rate=22050, stereo=True, wave_type='sine', volume=1.0):
    """
    Membuat buffer looping sempurna dengan mencari siklus kelipatan terkecil
    sehingga ujung awal dan akhir buffer kontinu (bebas suara 'pop' / 'click').
    """
    if freq <= 0:
        return bytearray(512), 128  # Hening / Pause

    # Cari M (jumlah siklus) dan N (jumlah sampel) yang mendekati bilangan bulat
    best_m = 1
    best_n = max(2, round(sample_rate / freq))
    best_err = abs(best_n - (sample_rate / freq))

    for m in range(1, 25):
        n = round(m * sample_rate / freq)
        if n > 1024:
            break
        err = abs(n - (m * sample_rate / freq)) / m
        if err < best_err:
            best_m = m
            best_n = n
            best_err = err
            if err < 0.005:
                break

    amplitude = int(32767 * max(0.0, min(1.0, volume)))
    samples = []

    for i in range(best_n):
        t = i / best_n
        phase = (best_m * t) % 1.0

        if wave_type == 'sine':
            val = math.sin(2 * math.pi * best_m * t)
        elif wave_type == 'square':
            val = 1.0 if phase < 0.5 else -1.0
        elif wave_type == 'triangle':
            val = (4 * phase - 1) if phase < 0.5 else (3 - 4 * phase)
        elif wave_type == 'sawtooth':
            val = 2 * phase - 1
        else:
            val = math.sin(2 * math.pi * best_m * t)

        samples.append(int(val * amplitude))

    # Kemas ke format 16-bit little-endian
    bytes_per_frame = 4 if stereo else 2
    buf = bytearray(best_n * bytes_per_frame)
    fmt = '<hh' if stereo else '<h'

    for i, s in enumerate(samples):
        offset = i * bytes_per_frame
        if stereo:
            struct.pack_into(fmt, buf, offset, s, s)
        else:
            struct.pack_into(fmt, buf, offset, s)

    return buf, best_n

def play_tone(dac, freq, duration_ms, wave_type='sine', volume=None):
    """
    Memainkan sebuah nada tunggal pada frekuensi tertentu selama durasi (ms).
    """
    if volume is None:
        volume = dac.get_volume()

    if freq <= 0:
        # Istirahat / REST
        time.sleep_ms(int(duration_ms))
        return

    buf, n_samples = generate_tone_buffer(
        freq,
        sample_rate=dac.sample_rate,
        stereo=dac.stereo,
        wave_type=wave_type,
        volume=volume
    )

    total_samples = int(dac.sample_rate * (duration_ms / 1000.0))
    loops = max(1, total_samples // n_samples)

    for _ in range(loops):
        dac.write(buf)

def play_melody(dac, melody, wave_type='sine', default_duration=250):
    """
    Memainkan sekumpulan not musik.
    Setiap elemen pada list melody berupa tuple: (NAMA_NADA, DURASI_MS, JEDA_MS)
    atau string NAMA_NADA saja.
    """
    for item in melody:
        if isinstance(item, tuple):
            note = item[0]
            dur = item[1] if len(item) > 1 else default_duration
            pause = item[2] if len(item) > 2 else 20
        else:
            note = item
            dur = default_duration
            pause = 20

        freq = NOTES.get(note.upper(), 0) if isinstance(note, str) else float(note)

        play_tone(dac, freq, dur, wave_type=wave_type)
        if pause > 0:
            time.sleep_ms(int(pause))

def play_sweep(dac, start_freq, end_freq, duration_ms=1000, steps=30, wave_type='sine'):
    """
    Memainkan nada meluncur (frekuensi naik / turun) seperti suara laser / sci-fi / sirine.
    """
    step_duration = max(5, duration_ms // steps)
    for i in range(steps):
        t = i / steps
        freq = start_freq + (end_freq - start_freq) * t
        play_tone(dac, freq, step_duration, wave_type=wave_type)

# Contoh Melodi Populer
MELODY_SCALE = [
    ('C4', 300, 30), ('D4', 300, 30), ('E4', 300, 30), ('F4', 300, 30),
    ('G4', 300, 30), ('A4', 300, 30), ('B4', 300, 30), ('C5', 400, 50)
]

MELODY_CHIME = [
    ('E5', 120, 20), ('G5', 120, 20), ('E6', 200, 40), ('C6', 150, 20),
    ('D6', 150, 20), ('G6', 350, 60)
]

MELODY_COIN = [
    ('B5', 90, 10), ('E6', 300, 20)
]
