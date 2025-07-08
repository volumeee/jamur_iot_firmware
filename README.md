# Jamur IoT PIO

Firmware ESP32 untuk kontrol otomatis budidaya jamur tiram, dengan monitoring kelembapan, suhu, dan kontrol pompa air berbasis MQTT, WiFi, dan LCD.

---

## Fitur Utama

- Monitoring suhu & kelembapan otomatis (DHT11)
- Kontrol pompa otomatis (berdasarkan kelembapan & jadwal)
- Notifikasi MQTT (aksi, warning, info, periodik, error)
- Web setup WiFi (AP mode) dengan validasi input
- LCD I2C 16x2 untuk status
- OTA update via MQTT (dengan pengecekan free space)
- Penyimpanan konfigurasi di flash (Preferences) via helper class
- Retry notifikasi jika gagal
- Non-blocking MQTT reconnect (device selalu responsif)
- Debounce tombol (anti input ganda)
- Validasi jadwal siram (jam 0-23, max 5 jadwal)
- Clean code & best practice (OOP, DRY, error handling robust)

---

## Hardware yang Dibutuhkan

- ESP32 Dev Board
- Sensor DHT11 (kelembapan & suhu)
- Relay 1 channel (untuk pompa)
- Pompa air 5V/12V
- LCD I2C 16x2
- Tombol push (UP, DOWN, OK, BACK)
- Koneksi WiFi

### Pinout Default

| Fungsi   | Pin ESP32 |
| -------- | --------- |
| DHT11    | 4         |
| Relay    | 23        |
| LCD SDA  | default   |
| LCD SCL  | default   |
| BTN_UP   | 19        |
| BTN_DOWN | 18        |
| BTN_OK   | 5         |
| BTN_BACK | 17        |

---

## Wiring Sederhana

```
DHT11   -> GPIO4
Relay   -> GPIO23
BTN_UP  -> GPIO19
BTN_DOWN-> GPIO18
BTN_OK  -> GPIO5
BTN_BACK-> GPIO17
LCD I2C -> default SDA/SCL ESP32
```

---

## Setup Software

1. **Install [PlatformIO](https://platformio.org/)** (VSCode recommended)
2. Clone repo ini:
   ```bash
   git clone <repo-url>
   cd Jamur_IoT_PIO
   ```
3. Edit file `include/secrets.h` (tidak di-repo) untuk mengisi:
   ```cpp
   #define SECRET_WIFI_SSID "yourssid"
   #define SECRET_WIFI_PASS "yourpass"
   #define SECRET_AP_SSID   "Jamur-Setup"
   #define SECRET_AP_PASS   "12345678"
   #define SECRET_MQTT_BROKER "broker.hivemq.com"
   #define SECRET_MQTT_USER   "user"
   #define SECRET_MQTT_PASS   "pass"
   ```
4. Build & upload ke ESP32:
   ```bash
   pio run -t upload
   ```
5. Monitor serial:
   ```bash
   pio device monitor
   ```

---

## Konfigurasi & Penggunaan

- **Setup WiFi:**
  - Tahan tombol BACK saat boot untuk masuk mode AP.
  - Hubungkan ke WiFi AP perangkat, buka http://192.168.4.1, isi SSID & password.
  - **Validasi:** SSID max 32 karakter, password max 64 karakter.
- **Normal Mode:**
  - Perangkat akan otomatis konek WiFi & MQTT, tampilkan status di LCD.
- **Tombol:**
  - OK (tekan singkat): Info perangkat
  - OK (tekan lama): Siram manual
  - BACK: Kembali ke mode normal
  - **Debounce:** Semua tombol anti input ganda

---

## MQTT Topics

| Topic                  | Tipe      | Keterangan                            |
| ---------------------- | --------- | ------------------------------------- |
| jamur/telemetry        | publish   | Data suhu & kelembapan                |
| jamur/status           | publish   | Status pompa (idle/pumping)           |
| jamur/notifications    | publish   | Notifikasi aksi, warning, info, error |
| jamur/control/pump     | subscribe | Perintah ON (manual)                  |
| jamur/config/get       | publish   | Konfigurasi saat ini                  |
| jamur/config/set       | subscribe | Update konfigurasi dari MQTT          |
| jamur/wifi_signal      | publish   | Sinyal WiFi (RSSI)                    |
| jamur/system/update    | subscribe | Perintah OTA update                   |
| jamur/firmware/current | publish   | Versi firmware                        |

### Contoh Notifikasi

```json
{
  "type": "info",
  "message": "Pump turned ON (auto_critical).",
  "humidity": 78.5,
  "temperature": 27.1
}
```

### Contoh Notifikasi Error

```json
{
  "type": "error",
  "message": "OTA update failed: not enough space."
}
```

---

## Validasi Input & Keamanan

- **SSID:** max 32 karakter, tidak boleh kosong
- **Password:** max 64 karakter
- **Jadwal siram:** hanya jam 0-23, max 5 jadwal, input invalid diabaikan
- **Semua input tervalidasi sebelum disimpan ke flash**

---

## OTA Update via MQTT

- Kirim ke `jamur/system/update` payload:

```json
{ "command": "FIRMWARE_UPDATE", "url": "https://your-server/firmware.bin" }
```

- Jika sukses, device akan restart otomatis.
- **Keamanan:** Device cek free space sebelum update, error akan dikirim ke notifikasi jika gagal.

---

## Troubleshooting

- **Tidak bisa konek WiFi:**
  - Cek SSID/password, ulangi setup AP.
- **MQTT gagal:**
  - Cek broker, user/pass, port, sertifikat CA.
- **Sensor tidak terbaca:**
  - Cek wiring DHT11.
- **Pompa tidak nyala:**
  - Cek wiring relay & power supply.
- **Notifikasi tidak muncul:**
  - Cek koneksi MQTT, cek serial log.
- **OTA gagal:**
  - Cek free space, cek URL firmware, cek notifikasi error.

---

## Konfigurasi Lanjutan

- Semua interval, retry, dan pin dapat diubah di `include/config.h`.
- Jadwal siram dapat diatur via MQTT topic `jamur/config/set`.
- Semua konfigurasi disimpan dan di-load via helper class agar DRY dan aman.

---

## Kontak & Lisensi

- Author: [bagus-erwanto.vercel.app](https://bagus-erwanto.vercel.app)
- Telegram: @baguserwanto
- Lisensi: MIT
