# SimFlow Locator (`sf_locator`)

A portable tracker device (codename **simpleBot**) built on ESP32-S3 that
detects when it is **picked up**, when it is **moved**, and when its IMU
**overheats**, and reports those events to the user over **Telegram**. The
device also exposes a local **Wi-Fi Soft-AP + web UI** for configuration
(Wi-Fi credentials, MPU baseline calibration, LED control).

## Hardware

* **MCU:** ESP32-S3 (DevKitC-1 or compatible)
* **Sensor:** MPU6050 IMU on I2C (SDA=GPIO 1, SCL=GPIO 2)
* **LED:** WS2812 NeoPixel on GPIO 48 (status indicator)
* **Input:** BOOT button on GPIO 0 (single / double / triple-click)

## Behaviour Summary

| Event                                   | Reaction                                            |
|-----------------------------------------|-----------------------------------------------------|
| Boot                                    | Solid red LED, "simpleBot started" sent to Telegram |
| MPU baseline-Az drop > 2.5 m/s²         | "Lift detected" Telegram alert (3 s cooldown)       |
| Any movement in a rolling 1-min window  | Motion summary Telegram message at window end       |
| MPU temperature ≥ 60 °C / ≤ 55 °C       | Overheat warning / recovery Telegram                |
| Wi-Fi STA lost                          | LED alternates red/blue until triple-click dismiss  |
| BOOT single-click                       | Cycle LED color (red → green → blue → off → …)      |
| BOOT double-click                       | Trigger chip-temperature message                    |
| BOOT triple-click (only in Wi-Fi-lost)  | Dismiss the alert pattern                           |

The device serves an HTML control page on port 80 from both the STA and the
Soft-AP interface. The page is plain HTML/JS and is **embedded into the
firmware binary at build time** from
[`locator_app/webui/index.html`](locator_app/webui/index.html).

## Folder Structure

```
device/sf_locator/
├── README.md               # this file
├── CMakeLists.txt          # IDF component registration + EMBED_TXTFILES
│
├── locator_fw/             # FIRMWARE LEVEL (C, runs on ESP32-S3)
│   ├── config.h            # credential template (gitignored before flash)
│   ├── sf_locator.{c,h}    # init_device() / device_start() / Wi-Fi APSTA / loop task
│   ├── sf_locator_led.{c,h}      # WS2812 driver + blink / wifi-lost pattern
│   ├── sf_locator_mpu.{c,h}      # MPU6050 I2C driver + lift / motion / overheat
│   ├── sf_locator_button.{c,h}   # debounced single / double / triple-click
│   ├── sf_locator_telegram.{c,h} # HTTPS sendMessage via mbedtls cert bundle
│   └── sf_locator_web.{c,h}      # esp_http_server + Soft-AP + NVS
│
└── locator_app/            # APPLICATION LEVEL (host-side / user-facing)
    ├── SimFlow_HLD.md      # high-level design (architecture, module table)
    ├── integration_guide.md# build, first boot, HTTP API table, alert table
    └── webui/
        └── index.html      # on-device control page, embedded at build time
```

The split mirrors `device/sf_watering/`:

| `sf_watering`        | `sf_locator`        | What lives here                  |
|----------------------|---------------------|----------------------------------|
| `watering_fw/`       | `locator_fw/`       | ESP-IDF C firmware sources       |
| `watering_app/`      | `locator_app/`      | Docs + user-facing UI artifacts  |
| `watering_app/server/` | *(not present)*   | Locator has no host middleware - it talks straight to Telegram and serves its own UI |

## Module Map

| Concern                           | File                                 |
|-----------------------------------|--------------------------------------|
| Device lifecycle (`init` / `start`) | [sf_locator.c](locator_fw/sf_locator.c) |
| Wi-Fi STA + Soft-AP + watchdog     | [sf_locator.c](locator_fw/sf_locator.c) |
| LED effects                        | [sf_locator_led.c](locator_fw/sf_locator_led.c) |
| MPU6050 driver + event detection   | [sf_locator_mpu.c](locator_fw/sf_locator_mpu.c) |
| Button debouncing + click logic    | [sf_locator_button.c](locator_fw/sf_locator_button.c) |
| Telegram HTTPS notifications       | [sf_locator_telegram.c](locator_fw/sf_locator_telegram.c) |
| HTTP server + NVS + AP credentials | [sf_locator_web.c](locator_fw/sf_locator_web.c) |
| Control page (HTML/JS)             | [locator_app/webui/index.html](locator_app/webui/index.html) |

## Build

From the SimFlow repo root:

```bash
idf.py set-target esp32s3       # one-time
idf.py build
idf.py -p COMx flash monitor
```

Fill in [`locator_fw/config.h`](locator_fw/config.h) before flashing
(`WIFI_SSID`, `WIFI_PASSWORD`, `BOT_TOKEN`, `CHAT_ID`). Empty `BOT_TOKEN` /
`CHAT_ID` make Telegram calls no-ops; everything else still works.

See [`locator_app/integration_guide.md`](locator_app/integration_guide.md)
for the HTTP API reference and per-feature usage,
and [`locator_app/SimFlow_HLD.md`](locator_app/SimFlow_HLD.md) for the
architectural overview.
