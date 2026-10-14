# Raspberry Pi Pico - USB Audio DAC Sound Card (PCM5102A)

Proyek ini mengubah **Raspberry Pi Pico (RP2040)** menjadi **USB Sound Card eksternal (USB Audio DAC)** berkualitas tinggi untuk Komputer / Laptop (Windows, macOS, Linux, atau Android).

Audio digital dari komputer (seperti Spotify, YouTube, game, atau file musik) dialirkan melalui kabel USB ke Pico secara *real-time* (48 kHz / 16-bit Stereo), kemudian diubah menjadi sinyal **I2S** oleh RP2040 dan diteruskan ke modul DAC **PCM5102A** (modul nomor 2) untuk menghasilkan suara analog jernih ke jack 3.5mm.

---

## 🎧 Arsitektur Sistem

```text
[ Komputer / Laptop ]
         │  (Kabel USB - USB Audio Class 2.0 Streaming)
         ▼
[ Raspberry Pi Pico (RP2040) ]
   ├── TinyUSB Audio Device Stack (Isochronous OUT + Feedback EP)
   ├── Ring Buffer Thread-Safe (4096 bytes)
   └── PIO I2S Transmitter (GP16, GP17, GP18)
         │  (Protokol I2S: BCLK, LRCK, DIN)
         ▼
[ Modul DAC PCM5102A (Modul No. 2) ]
         │  (Audio Analog Stereo)
         ▼
[ Headphone / Speaker Aktif (Jack 3.5mm) ]
```

---

## 📌 1. Skema Pengkabelan (Wiring Diagram PCM5102A)

Hubungkan pin Raspberry Pi Pico ke modul DAC **PCM5102A** sebagai berikut:

| Pin Raspberry Pi Pico | Pin Fisik Pico | Pin PCM5102A | Keterangan & Catatan Jumper Khusus |
| :--- | :--- | :--- | :--- |
| **3V3(OUT)** atau **VBUS** | Pin 36 / Pin 40 | **VCC / VIN** | Daya 3.3V atau 5V |
| **GND** | Pin 38 atau Pin 3 | **GND** | Ground bersama |
| **GP16** | Pin 21 | **BCK** | Bit Clock (BCLK) |
| **GP17** | Pin 22 | **LCK** | Word Clock / LRCK (Wajib GP16 + 1) |
| **GP18** | Pin 24 | **DIN** | Data Audio In |
| **GND** | Pin 38 | **SCK** | **KRUSIAL:** Wajib ke GND agar chip mengaktifkan osilator PLL internal dari BCK |
| **GND** | Pin 38 | **FMT** | Wajib ke GND untuk format standar protokol I2S |
| **3V3(OUT)** | Pin 36 | **XMT / DMP** | Wajib ke 3.3V (High = Un-mute suara) |
| **GND** | Pin 38 | **FLT** | Hubungkan ke GND (filter latensi normal) |
| - | - | **Jack 3.5mm** | Sambungkan ke Earphone, Headphone, atau Speaker |

> [!IMPORTANT]
> Pada banyak modul ungu PCM5102A, perhatikan solder jumper di bagian belakang board (H1L, H2L, H3L, H4L):
> - **SCK** harus terhubung ke **GND** (Low).
> - **XMT** harus terhubung ke **3.3V** (High / Unmute).

---

## 🛠️ 2. Cara Melakukan Build (Menghasilkan `.uf2`)

### Langkah 1: Instalasi Toolchain Kompilasi (Sekali saja)
Jika Anda menggunakan **Arch Linux / Manjaro**:
```bash
sudo pacman -S cmake arm-none-eabi-gcc arm-none-eabi-newlib
```

Jika menggunakan **Ubuntu / Debian / Raspberry Pi OS**:
```bash
sudo apt update
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential
```

### Langkah 2: Jalankan Skrip Build
Di terminal, jalankan:
```bash
./build.sh
```

Skrip akan secara otomatis mengompilasi firmware dan menghasilkan berkas:
```text
build/pico_usb_sound_card.uf2
```

---

## ⚡ 3. Cara Flash ke Raspberry Pi Pico

1. Tekan dan tahan tombol **BOOTSEL** pada Raspberry Pi Pico.
2. Sambungkan kabel USB Pico ke komputer, lalu lepaskan tombol BOOTSEL.
3. Drive penyimpanan bernama **`RPI-RP2`** akan muncul seperti flashdisk.
4. Salin / drag-and-drop berkas **`build/pico_usb_sound_card.uf2`** ke drive `RPI-RP2`.
5. Pico akan otomatis restart.

---

## 🎵 4. Cara Penggunaan di Komputer

1. Setelah Pico di-flash, biarkan kabel USB tetap terhubung ke komputer.
2. Buka pengaturan audio di OS Anda:
   - **Windows:** Klik ikon speaker di taskbar kanan bawah $\rightarrow$ pilih **"Pico PCM5102A USB DAC"**.
   - **Linux:** Buka *Sound Settings* atau jalankan `pavucontrol` $\rightarrow$ pilih **"Pico PCM5102A USB DAC"**.
   - **macOS:** Buka *System Settings* $\rightarrow$ *Sound* $\rightarrow$ pilih **"Pico PCM5102A USB DAC"**.
3. Putar musik di YouTube atau Spotify.
4. Lampu LED onboard pada Pico akan menyala terus saat audio sedang dialirkan.
5. Suara akan terdengar langsung melalui headphone/speaker yang tercolok ke modul PCM5102A!
6. Penggeser volume (volume slider) di komputer Anda juga berfungsi secara otomatis mengatur volume audio pada Pico.

---

## 📂 5. Struktur Berkas Proyek

```text
RaspBerry/
├── pico-sdk/              # Repositori resmi Raspberry Pi Pico SDK
├── pico_sdk_import.cmake  # Skrip resmi pengimpor SDK untuk CMake
├── CMakeLists.txt         # Konfigurasi build CMake untuk firmware USB Sound Card
├── build.sh               # Skrip shell otomatis untuk build .uf2
├── tusb_config.h          # Konfigurasi TinyUSB Audio Class 2.0 Speaker
├── usb_descriptors.h      # Definisi descriptor antarmuka audio & endpoint
├── usb_descriptors.c      # Implementasi descriptor USB perangkat
├── ring_buffer.h          # Lock-free Ring Buffer audio stereo
├── audio_i2s.pio          # Program PIO assembly untuk transmisi I2S 16-bit stereo
├── audio_i2s.h            # Header driver C I2S audio
├── audio_i2s.c            # Implementasi driver C I2S audio
├── main.c                 # Program utama USB Sound Card (Core 0 USB + I2S Stream)
└── README.md              # Dokumentasi lengkap proyek
```
