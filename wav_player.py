"""
WAV Audio Player untuk Raspberry Pi Pico I2S DAC
Membaca dan memainkan file audio WAV (PCM tak terkompresi 16-bit)
secara streaming hemat memori (buffer chunking).
"""

import struct

def parse_wav_header(file_obj):
    """
    Menganalisis header file WAV dan mengembalikan informasi audio:
    channels, sample_rate, bits_per_sample, dan data_size.
    """
    header = file_obj.read(12)
    if len(header) < 12 or header[:4] != b'RIFF' or header[8:12] != b'WAVE':
        raise ValueError("Format file tidak valid: Harus berupa file audio RIFF/WAVE.")

    channels = 2
    sample_rate = 22050
    bits_per_sample = 16
    data_size = 0
    audio_format = 1

    while True:
        chunk_hdr = file_obj.read(8)
        if len(chunk_hdr) < 8:
            break

        chunk_id = chunk_hdr[:4]
        chunk_size = struct.unpack('<I', chunk_hdr[4:8])[0]

        if chunk_id == b'fmt ':
            fmt_data = file_obj.read(chunk_size)
            if len(fmt_data) < 16:
                raise ValueError("Format chunk 'fmt' terlalu pendek.")
            audio_format, channels, sample_rate, byte_rate, block_align, bits_per_sample = struct.unpack(
                '<HHIIHH', fmt_data[:16]
            )
        elif chunk_id == b'data':
            data_size = chunk_size
            break
        else:
            # Lewati chunk metadata / LIST / ID3 yang tidak digunakan
            remaining = chunk_size
            while remaining > 0:
                step = min(remaining, 512)
                buf = file_obj.read(step)
                if not buf:
                    break
                remaining -= len(buf)

    if audio_format != 1:
        raise ValueError(f"Hanya format PCM standar (format 1) yang didukung. Ditemukan format {audio_format}.")

    if bits_per_sample != 16:
        raise ValueError(f"Hanya audio 16-bit PCM yang didukung saat ini (ditemukan {bits_per_sample}-bit).")

    return {
        'channels': channels,
        'sample_rate': sample_rate,
        'bits_per_sample': bits_per_sample,
        'data_size': data_size
    }

def play_wav(dac, filepath, chunk_size=1024):
    """
    Memainkan file WAV langsung dari storage ke I2S DAC secara streaming.
    - dac: instance I2SDAC
    - filepath: path ke file .wav (misal 'sample.wav')
    - chunk_size: ukuran chunk pembacaan buffer (hemat RAM)
    """
    print(f"Membuka file WAV: {filepath} ...")
    with open(filepath, 'rb') as f:
        info = parse_wav_header(f)
        channels = info['channels']
        sample_rate = info['sample_rate']
        bits = info['bits_per_sample']

        print(f"Info WAV: {channels} Channel, {sample_rate} Hz, {bits}-bit PCM")

        is_stereo = (channels == 2)
        # Konfigurasikan sample rate DAC agar sinkron dengan file WAV
        dac.reconfigure(sample_rate=sample_rate, stereo=True)

        bytes_read = 0
        target_size = info['data_size']

        # Buffer pembacaan
        while True:
            if target_size > 0 and bytes_read >= target_size:
                break

            to_read = chunk_size
            if target_size > 0:
                to_read = min(chunk_size, target_size - bytes_read)

            raw = f.read(to_read)
            if not raw:
                break

            bytes_read += len(raw)

            # Jika file aslinya mono, duplikasi ke channel Kiri & Kanan (Stereo)
            if channels == 1:
                # 2 byte per sampel mono -> 4 byte per sampel stereo
                num_samples = len(raw) // 2
                stereo_buf = bytearray(num_samples * 4)
                for i in range(num_samples):
                    s = raw[i * 2 : (i + 1) * 2]
                    stereo_buf[i * 4 : i * 4 + 2] = s
                    stereo_buf[i * 4 + 2 : (i + 1) * 4] = s
                dac.write_with_volume(stereo_buf)
            else:
                dac.write_with_volume(raw)

    print("Selesai memutar WAV.")
