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

# ESP-NOW Slave — Sensor & Weather Client

Firmware ESP-NOW slave untuk perangkat sensor yang meminta data cuaca melalui master-proxy, lalu mengirimkan state terstruktur kembali ke master.

Highlights:
- ESP-NOW slave (channel scan, master lock, peer register)
- Mengirim `Identity`, `Sensor`, `Weather`, dan `SlaveAlive` state
- Proxy request/response untuk mengambil data Open-Meteo melalui master

Quick Links:
- Entrypoint: `src/main.cpp`
- ESP-NOW logic: `src/app/espnow/slave.cpp`
- Weather pipeline: `src/app/espnow/weather_pipeline.cpp`
- Weather locations: `src/app/weather/open_meteo_locations.cpp`

Protocol summary:
- Wire structs: `src/app/espnow/state_binary.h`
- Outbound (`PacketType::STATE`): `IdentityState`, `SensorState`, `WeatherState`, `SlaveAliveState`, `ProxyReqState`
- Inbound (`PacketType::COMMAND`): `ProxyRespChunkCommand`, `WeatherSyncReqCommand`

Configuration:
- Edit `include/app_config.h` for device identity and feature toggles (`DEVICE_NAME`, DHT settings, `WEATHER_AREA_INDEX`, intervals).

Supported build environments (examples):
- `wemos-lolin32-lite`
- `esp32-c3-super-mini`

Build & flash (example):
```bash
# build
platformio run -e wemos-lolin32-lite
# upload (replace port with your device)
platformio run -e wemos-lolin32-lite -t upload --upload-port /dev/ttyUSB0
# monitor serial
platformio device monitor -e wemos-lolin32-lite --port /dev/ttyUSB0
```

Notes:
- Slave only accepts packets from a validated master beacon.
- If master times out, slave returns to channel-scan mode.
- Weather proxy responses arrive as chunks and are reassembled by `weather_pipeline`.

License & contribution
- See repository root for licensing and contribution guidance.

---

If mau, saya bisa tambahkan petunjuk environment build per-board atau contoh konfigurasi `include/secret.h` (tanpa secrets).
