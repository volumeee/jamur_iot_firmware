# Jamur IoT - Firmware Kontrol Budidaya Jamur Tiram

Firmware ESP32 untuk sistem kontrol otomatis budidaya jamur tiram dengan monitoring kelembapan, suhu, kontrol pompa, notifikasi MQTT, dan OTA update.

## 🚀 Fitur Utama

- **Monitoring Real-time**: Kelembapan dan suhu dengan sensor DHT11
- **Kontrol Otomatis**: Pompa air berdasarkan ambang batas kelembapan
- **Penjadwalan**: Siram otomatis berdasarkan jadwal yang dapat dikonfigurasi
- **Notifikasi**: Email dan MQTT untuk alert dan status
- **Web Interface**: Setup WiFi via Access Point
- **OTA Update**: Update firmware over-the-air
- **MQTT Integration**: Komunikasi dengan broker MQTT aman (TLS)
- **Supabase Integration**: Email notifikasi via Edge Functions

## 📋 Persyaratan Hardware

- ESP32 Development Board
- Sensor DHT11/DHT22
- Relay module untuk pompa air
- LCD I2C 16x2
- 4 tombol (UP, DOWN, OK, BACK)
- Power supply 5V/2A

## 🔧 Setup Development

### 1. Install PlatformIO

```bash
# Install PlatformIO Core
pip install platformio

# Atau gunakan PlatformIO IDE di VS Code
```

### 2. Clone Repository

```bash
git clone https://github.com/your-username/Jamur_IoT_PIO.git
cd Jamur_IoT_PIO
```

### 3. Setup Secrets

Buat file `include/secrets.h` dengan kredensial Anda:

```cpp
#pragma once

// ==========================================================
// ==      FILE RAHASIA - JANGAN DI-UPLOAD KE GITHUB       ==
// ==========================================================

#define SECRET_WIFI_SSID "your_wifi_ssid"
#define SECRET_WIFI_PASS "your_wifi_password"

#define SECRET_AP_SSID "JamurIoT_Setup"
#define SECRET_AP_PASS "jamur123"

#define SECRET_MQTT_BROKER "your_mqtt_broker.hivemq.cloud"
#define SECRET_MQTT_USER "your_mqtt_username"
#define SECRET_MQTT_PASS "your_mqtt_password"

#define SECRET_SUPABASE_URL "https://your-project.supabase.co"
#define SECRET_SUPABASE_KEY "your_supabase_anon_key"
```

### 4. Build dan Upload

```bash
# Build project
pio run

# Upload ke ESP32
pio run --target upload

# Monitor serial
pio device monitor
```

## 🔐 Setup GitHub Secrets (untuk CI/CD)

Untuk build otomatis di GitHub Actions, tambahkan secrets berikut di repository settings:

1. Buka **Settings** > **Secrets and variables** > **Actions**
2. Tambahkan secrets berikut:

| Secret Name    | Description                      |
| -------------- | -------------------------------- |
| `WIFI_SSID`    | SSID WiFi untuk testing          |
| `WIFI_PASS`    | Password WiFi untuk testing      |
| `AP_SSID`      | SSID untuk mode Access Point     |
| `AP_PASS`      | Password untuk mode Access Point |
| `MQTT_BROKER`  | URL broker MQTT                  |
| `MQTT_USER`    | Username MQTT                    |
| `MQTT_PASS`    | Password MQTT                    |
| `SUPABASE_URL` | URL Supabase project             |
| `SUPABASE_KEY` | Anon key Supabase                |

## 🛠️ Troubleshooting

### Masalah SSID Tidak Ditemukan

Jika muncul error `SSID too long or missing!`:

1. **Periksa file secrets.h**: Pastikan `SECRET_WIFI_SSID` tidak kosong
2. **Reset Preferences**: Tekan tombol BACK saat boot untuk masuk mode AP
3. **Setup ulang WiFi**: Hubungkan ke AP dan masukkan kredensial WiFi

### Debug Log

Firmware akan menampilkan log debug di Serial Monitor:

```
Inisialisasi Hardware...
Memuat konfigurasi...
Konfigurasi dimuat.
Inisialisasi Penyimpanan dan WiFi...
MQTT Client ID: jamur-iot-2B2034
Stored SSID: 'your_wifi_ssid'
Stored PASS: ***
Menggunakan SSID: your_wifi_ssid
Mencoba koneksi ke WiFi: your_wifi_ssid
WiFi terhubung!
IP Address: 192.168.1.100
```

### Mode Access Point

Jika WiFi tidak tersimpan atau gagal koneksi:

1. Tekan dan tahan tombol **BACK** saat boot
2. Hubungkan ke WiFi `JamurIoT_Setup` dengan password `jamur123`
3. Buka browser dan akses `http://192.168.4.1`
4. Masukkan kredensial WiFi Anda
5. Perangkat akan restart dan mencoba koneksi

## 📡 MQTT Topics

| Topic                          | Direction | Description                               |
| ------------------------------ | --------- | ----------------------------------------- |
| `jamur/telemetry`              | Publish   | Data sensor (humidity, temperature)       |
| `jamur/status`                 | Publish   | Status perangkat (online/offline/pumping) |
| `jamur/notifications`          | Publish   | Notifikasi sistem                         |
| `jamur/control/pump`           | Subscribe | Kontrol pompa (ON/OFF)                    |
| `jamur/config/get`             | Publish   | Konfigurasi saat ini                      |
| `jamur/config/set`             | Subscribe | Update konfigurasi                        |
| `jamur/wifi_signal`            | Publish   | Sinyal WiFi (RSSI)                        |
| `jamur/system/update`          | Subscribe | Command update firmware                   |
| `jamur/firmware/current`       | Publish   | Versi firmware saat ini                   |
| `jamur/firmware/new_available` | Subscribe | Notifikasi firmware baru                  |

## 🔧 Konfigurasi

### Ambang Batas Kelembapan

- **Critical**: 80% (pompa otomatis ON)
- **Warning**: 85% (peringatan)
- **Default**: Dapat diubah via MQTT atau web interface

### Jadwal Siram

Default: 07:00, 12:00, 17:00
Dapat dikonfigurasi via MQTT topic `jamur/config/set`

### Durasi Pompa

Default: 30 detik
Dapat diubah di `config.h` - `PUMP_DURATION_MS`

## 📧 Email Notifikasi

Firmware mengirim email notifikasi via Supabase Edge Functions untuk:

- **Critical Alert**: Kelembapan di bawah ambang batas kritis
- **Warning**: Kelembapan mendekati ambang batas
- **Info**: Kondisi kembali normal
- **Firmware Update**: Tersedia update firmware baru

## 🔄 OTA Update

Firmware mendukung update over-the-air:

1. Upload firmware baru ke server
2. Kirim command via MQTT topic `jamur/system/update`
3. Firmware akan download dan install otomatis
4. Perangkat restart dengan firmware baru

## 📝 Changelog

### v23.3 (Latest)

- ✅ Perbaikan notifikasi hanya saat status berubah
- ✅ Helper function untuk publish MQTT dengan retry
- ✅ Notifikasi periodik setiap 1 menit
- ✅ Validasi input SSID dan password
- ✅ Helper class untuk Preferences (DRY principle)
- ✅ Email notifikasi terpusat dengan struct NotificationData
- ✅ Pengecekan free space sebelum OTA update
- ✅ MQTT reconnect non-blocking
- ✅ Keamanan: secrets.h tidak ter-commit ke GitHub
- ✅ CI/CD workflow dengan generate secrets otomatis

### v17.3

- ArduinoJson v7 compatibility fix
- Perbaikan struktur kode

## 🤝 Kontribusi

1. Fork repository
2. Buat branch fitur baru (`git checkout -b feature/AmazingFeature`)
3. Commit perubahan (`git commit -m 'Add some AmazingFeature'`)
4. Push ke branch (`git push origin feature/AmazingFeature`)
5. Buat Pull Request

## 📄 Lisensi

Distributed under the MIT License. See `LICENSE` for more information.

## 👨‍💻 Author

**Bagus Erwanto**

- Website: [bagus-erwanto.vercel.app](https://bagus-erwanto.vercel.app)
- GitHub: [@bagus-erwanto](https://github.com/bagus-erwanto)

## 🙏 Acknowledgments

- PlatformIO untuk toolchain development
- HiveMQ untuk broker MQTT cloud
- Supabase untuk backend dan email service
- ArduinoJson untuk parsing JSON
- LiquidCrystal_I2C untuk LCD interface
