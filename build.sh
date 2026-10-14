#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=========================================================="
echo "    Build Raspberry Pi Pico - USB Sound Card (PCM5102A)   "
echo "=========================================================="

# 1. Cek keberadaan toolchain ARM dan CMake
MISSING_TOOLS=0
if ! command -v cmake &> /dev/null; then
    echo "[-] Tool 'cmake' belum terpasang."
    MISSING_TOOLS=1
fi

if ! command -v arm-none-eabi-gcc &> /dev/null; then
    echo "[-] Tool 'arm-none-eabi-gcc' (ARM Cortex Toolchain) belum terpasang."
    MISSING_TOOLS=1
fi

if [ $MISSING_TOOLS -ne 0 ]; then
    echo ""
    echo "[!] PERHATIAN: Toolchain kompilasi C/C++ belum lengkap di sistem Anda."
    echo "    Untuk memasang di Arch Linux, jalankan:"
    echo "      sudo pacman -S cmake arm-none-eabi-gcc arm-none-eabi-newlib"
    echo ""
    echo "    Untuk Ubuntu / Debian:"
    echo "      sudo apt update && sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential"
    echo "=========================================================="
    exit 1
fi

# 2. Cek direktori pico-sdk
if [ -d "$SCRIPT_DIR/pico-sdk" ]; then
    export PICO_SDK_PATH="$SCRIPT_DIR/pico-sdk"
    echo "[+] Menggunakan Pico SDK lokal: $PICO_SDK_PATH"
elif [ -n "$PICO_SDK_PATH" ]; then
    echo "[+] Menggunakan PICO_SDK_PATH lingkungan: $PICO_SDK_PATH"
else
    echo "[-] pico-sdk tidak ditemukan. Mengkloning pico-sdk..."
    git clone --depth 1 https://github.com/raspberrypi/pico-sdk.git "$SCRIPT_DIR/pico-sdk"
    export PICO_SDK_PATH="$SCRIPT_DIR/pico-sdk"
    cd "$SCRIPT_DIR/pico-sdk" && git submodule update --init lib/tinyusb && cd "$SCRIPT_DIR"
fi

# 3. Proses Kompilasi
echo "[+] Menyiapkan direktori build..."
mkdir -p build
cd build

echo "[+] Menjalankan CMake..."
cmake -DPICO_SDK_PATH="$PICO_SDK_PATH" ..

echo "[+] Membangun firmware dengan Make..."
make -j"$(nproc 2>/dev/null || echo 2)"

echo ""
echo "=========================================================="
if [ -f "pico_usb_sound_card.uf2" ]; then
    mkdir -p "$SCRIPT_DIR/firmware"
    cp pico_usb_sound_card.uf2 "$SCRIPT_DIR/firmware/"
    cp pico_usb_sound_card.uf2 "$SCRIPT_DIR/"
    echo " [SUKSES] File firmware UF2 berhasil dibuat dan diperbarui di repo:"
    echo "  -> $(pwd)/pico_usb_sound_card.uf2"
    echo "  -> $SCRIPT_DIR/firmware/pico_usb_sound_card.uf2"
    echo "  -> $SCRIPT_DIR/pico_usb_sound_card.uf2"
    echo ""

    echo " Cara Flash ke Raspberry Pi Pico:"
    echo " 1. Tekan dan tahan tombol BOOTSEL pada Raspberry Pi Pico."
    echo " 2. Pasang kabel USB ke komputer, lalu lepaskan BOOTSEL."
    echo " 3. Drive 'RPI-RP2' akan muncul seperti flashdisk."
    echo " 4. Salin / Drag-and-Drop file 'pico_usb_sound_card.uf2' ke drive RPI-RP2."
    echo " 5. Pico akan otomatis restart."
    echo " 6. Komputer akan mendeteksi: 'Pico PCM5102A USB DAC'."
    echo " 7. Pilih sebagai perangkat output audio di PC Anda, lalu putar musik!"
else
    echo " [!] Kompilasi selesai, periksa file output di direktori build."
fi
echo "=========================================================="
