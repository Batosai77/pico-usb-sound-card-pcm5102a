"""
Generator File Sample Audio WAV (16-bit PCM, 22050 Hz)
Dapat dijalankan di komputer (PC) maupun langsung di Raspberry Pi Pico MicroPython.
Menghasilkan nada musik arpeggio yang merdu untuk menguji kualitas audio DAC.
"""

import math
import struct

def generate_wav(output_path="sample.wav", duration=2.5, sample_rate=22050):
    print(f"Membuat file audio uji coba: {output_path} ({duration}s @ {sample_rate}Hz)...")

    # Sequence not musik: C5, E5, G5, B5, C6 (Arpeggio mayor 7th)
    notes = [
        (523.25, 0.0, 0.5),   # C5
        (659.25, 0.4, 0.9),   # E5
        (783.99, 0.8, 1.3),   # G5
        (987.77, 1.2, 1.7),   # B5
        (1046.50, 1.6, 2.5),  # C6
    ]

    total_samples = int(duration * sample_rate)
    pcm_data = bytearray(total_samples * 4)  # 16-bit stereo = 4 byte per frame

    for i in range(total_samples):
        t = i / sample_rate
        val = 0.0

        for freq, start, end in notes:
            if start <= t < end:
                note_t = t - start
                note_len = end - start

                # Envelope ADSR sederhana (Attack cepat, Decay eksponensial)
                if note_t < 0.02:
                    env = note_t / 0.02
                else:
                    env = math.exp(-3.5 * (note_t - 0.02) / note_len)

                # Gelombang sinus dengan sedikit harmonik ke-2 untuk timbre hangat
                tone = math.sin(2 * math.pi * freq * note_t) + 0.25 * math.sin(4 * math.pi * freq * note_t)
                val += tone * env * 0.45

        # Soft clipping / limiter
        val = max(-0.95, min(0.95, val))
        sample_16 = int(val * 32767)

        # Simpan ke stereo (L & R identik)
        offset = i * 4
        struct.pack_into('<hh', pcm_data, offset, sample_16, sample_16)

    # Susun Header WAV RIFF
    num_channels = 2
    bits_per_sample = 16
    byte_rate = sample_rate * num_channels * (bits_per_sample // 8)
    block_align = num_channels * (bits_per_sample // 8)
    data_size = len(pcm_data)
    chunk_size = 36 + data_size

    header = bytearray(44)
    struct.pack_into(
        '<4sI4s4sIHHIIHH4sI',
        header, 0,
        b'RIFF',
        chunk_size,
        b'WAVE',
        b'fmt ',
        16,             # Ukuran subchunk fmt
        1,              # Format PCM = 1
        num_channels,   # Stereo
        sample_rate,
        byte_rate,
        block_align,
        bits_per_sample,
        b'data',
        data_size
    )

    with open(output_path, "wb") as f:
        f.write(header)
        f.write(pcm_data)

    print(f"File berhasil dibuat: {output_path} ({len(header) + len(pcm_data)} bytes)")

if __name__ == "__main__":
    generate_wav("sample.wav")
