@echo off
REM Script setup development environment untuk Jamur IoT (Windows)

echo === Setup Development Environment Jamur IoT ===

REM Cek apakah secrets.h sudah ada
if not exist "include\secrets.h" (
    echo File secrets.h tidak ditemukan.
    echo Membuat file secrets.h dari contoh...
    
    if exist "include\secrets.example.h" (
        copy "include\secrets.example.h" "include\secrets.h"
        echo ✅ File secrets.h dibuat dari contoh
        echo ⚠️  Jangan lupa edit include\secrets.h dengan kredensial Anda!
    ) else (
        echo ❌ File secrets.example.h tidak ditemukan
        echo Buat file include\secrets.h manual dengan format:
        echo.
        echo #pragma once
        echo #define SECRET_WIFI_SSID "your_wifi_ssid"
        echo #define SECRET_WIFI_PASS "your_wifi_password"
        echo #define SECRET_AP_SSID "JamurIoT_Setup"
        echo #define SECRET_AP_PASS "jamur123"
        echo #define SECRET_MQTT_BROKER "your_mqtt_broker"
        echo #define SECRET_MQTT_USER "your_mqtt_user"
        echo #define SECRET_MQTT_PASS "your_mqtt_pass"
        echo #define SECRET_SUPABASE_URL "your_supabase_url"
        echo #define SECRET_SUPABASE_KEY "your_supabase_key"
    )
) else (
    echo ✅ File secrets.h sudah ada
)

REM Cek PlatformIO
pio --version >nul 2>&1
if %errorlevel% equ 0 (
    echo ✅ PlatformIO sudah terinstall
) else (
    echo ❌ PlatformIO belum terinstall
    echo Install dengan: pip install platformio
)

REM Cek dependencies
echo.
echo === Cek Dependencies ===
if exist "platformio.ini" (
    echo ✅ platformio.ini ditemukan
    echo Menjalankan: pio lib install
    pio lib install
) else (
    echo ❌ platformio.ini tidak ditemukan
)

echo.
echo === Setup Selesai ===
echo Langkah selanjutnya:
echo 1. Edit include\secrets.h dengan kredensial Anda
echo 2. Jalankan: pio run --target upload
echo 3. Monitor: pio device monitor

pause 