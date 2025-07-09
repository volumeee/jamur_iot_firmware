#!/bin/bash

# Script setup development environment untuk Jamur IoT
echo "=== Setup Development Environment Jamur IoT ==="

# Cek apakah secrets.h sudah ada
if [ ! -f "include/secrets.h" ]; then
    echo "File secrets.h tidak ditemukan."
    echo "Membuat file secrets.h dari contoh..."
    
    if [ -f "include/secrets.example.h" ]; then
        cp include/secrets.example.h include/secrets.h
        echo "✅ File secrets.h dibuat dari contoh"
        echo "⚠️  Jangan lupa edit include/secrets.h dengan kredensial Anda!"
    else
        echo "❌ File secrets.example.h tidak ditemukan"
        echo "Buat file include/secrets.h manual dengan format:"
        echo ""
        echo "#pragma once"
        echo "#define SECRET_WIFI_SSID \"your_wifi_ssid\""
        echo "#define SECRET_WIFI_PASS \"your_wifi_password\""
        echo "#define SECRET_AP_SSID \"JamurIoT_Setup\""
        echo "#define SECRET_AP_PASS \"jamur123\""
        echo "#define SECRET_MQTT_BROKER \"your_mqtt_broker\""
        echo "#define SECRET_MQTT_USER \"your_mqtt_user\""
        echo "#define SECRET_MQTT_PASS \"your_mqtt_pass\""
        echo "#define SECRET_SUPABASE_URL \"your_supabase_url\""
        echo "#define SECRET_SUPABASE_KEY \"your_supabase_key\""
    fi
else
    echo "✅ File secrets.h sudah ada"
fi

# Cek PlatformIO
if command -v pio &> /dev/null; then
    echo "✅ PlatformIO sudah terinstall"
else
    echo "❌ PlatformIO belum terinstall"
    echo "Install dengan: pip install platformio"
fi

# Cek dependencies
echo ""
echo "=== Cek Dependencies ==="
if [ -f "platformio.ini" ]; then
    echo "✅ platformio.ini ditemukan"
    echo "Menjalankan: pio lib install"
    pio lib install
else
    echo "❌ platformio.ini tidak ditemukan"
fi

echo ""
echo "=== Setup Selesai ==="
echo "Langkah selanjutnya:"
echo "1. Edit include/secrets.h dengan kredensial Anda"
echo "2. Jalankan: pio run --target upload"
echo "3. Monitor: pio device monitor" 