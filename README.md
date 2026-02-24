# ESP-NOW Slave (Sensor + Weather Client)

Firmware slave untuk mengirim sensor lokal, meminta data cuaca lewat proxy master, lalu mengirim hasil weather terstruktur kembali ke master.

## Ringkasan Peran

Slave bertugas untuk:
- Scan channel 1-13 sampai menemukan beacon master (`HELLO`/`HEARTBEAT`).
- Lock ke master yang valid (`MASTER_BEACON_ID`) dan register peer.
- Kirim state biner `Identity`, `Sensor`, dan `Weather` ke master.
- Kirim `ProxyReqState` untuk ambil data weather (Open-Meteo) via master.
- Menerima command chunk `ProxyRespChunk`, reassemble, parse `current_weather`, lalu publish `WeatherState`.
- Merespons command `WeatherSyncReq` dari master untuk refresh cuaca paksa.

## Arsitektur Runtime

- Entrypoint: `src/main.cpp`
- Node ESP-NOW: `src/app/espnow/slave.cpp`
   - Channel scan/lock, handshake master, heartbeat response (`SlaveAliveState`).
- Pipeline command weather async: `src/app/espnow/weather_pipeline.cpp`
   - Queue + task terpisah untuk parse chunk agar callback receive tetap ringan.
- Modul cuaca: `src/app/weather/open_meteo_locations.cpp`
   - Builder URL Open-Meteo + persist request terakhir di LittleFS.
- Sensor lokal: `src/app/sensor/dht_sensor.cpp`

## Protokol Saat Ini (Biner)

Payload utama memakai struct biner (`src/app/espnow/state_binary.h`):
- Ke master (`PacketType::STATE`): `IdentityState`, `SensorState`, `WeatherState`, `SlaveAliveState`, `ProxyReqState`
- Dari master (`PacketType::COMMAND`): `ProxyRespChunkCommand`, `WeatherSyncReqCommand`

`WeatherState` dikirim setelah field utama tersedia dari `current_weather`:
- `code`
- `time`
- `temperature`
- `windspeed`
- `winddirection`

## Alur Weather

1. Slave kirim `IdentityState` saat baru linked ke master.
2. Slave bangun URL Open-Meteo dari `WEATHER_AREA_INDEX`.
3. Slave kirim `ProxyReqState` (HTTP GET) ke master.
4. Master fetch internet dan kirim balik `ProxyRespChunkCommand`.
5. `weather_pipeline` menyusun chunk berurutan per `requestId`.
6. Pipeline parse `current_weather` lalu publish `WeatherState` ke master.

## Konfigurasi Utama

Edit `include/app_config.h`:
- Identitas: `DEVICE_NAME`
- DHT: `DHT_SENSOR_ENABLED`, `DHT_SENSOR_PIN`, `DHT_SENSOR_IS_DHT22`, `DHT_READ_INTERVAL_MS`
- Weather: `WEATHER_REPORT_ENABLED`, `WEATHER_AREA_INDEX`, `WEATHER_REPORT_INTERVAL_MS`, `WEATHER_PROXY_REQUEST_INTERVAL_MS`

## Build & Flash

Pilih environment board sesuai device:

```bash
platformio run -e wemos-lolin32-lite
platformio run -e wemos-lolin32-lite -t upload --upload-port /dev/ttyACM1
platformio device monitor -e wemos-lolin32-lite --port /dev/ttyACM1
```

Environment lain yang tersedia:
- `esp32-c3-super-mini`
- `wemos-lolin32-lite`

## Catatan Operasional

- Slave hanya menerima paket dari master yang beacon ID-nya valid.
- Jika master timeout, slave kembali ke mode scan channel otomatis.
- Request cuaca terakhir disimpan di `/data/weather_last_report.txt` untuk bootstrap berikutnya.
