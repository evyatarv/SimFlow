# SimFlow Locator Integration Guide

This guide describes how the locator firmware and the on-device web UI are
assembled, and how to bring the device up the first time.

## 1. Directory Structure

```text
device/sf_locator/
|-- CMakeLists.txt              # IDF component registration (firmware)
|-- locator_fw/                 # ESP32-S3 firmware (C)
|   |-- config.h                # credential template (gitignored)
|   |-- sf_locator.{c,h}        # entry points + loop task + wifi
|   |-- sf_locator_led.{c,h}    # WS2812 driver
|   |-- sf_locator_mpu.{c,h}    # MPU6050 driver + event logic
|   |-- sf_locator_button.{c,h} # click detection
|   |-- sf_locator_telegram.{c,h} # HTTPS sendMessage
|   `-- sf_locator_web.{c,h}    # HTTP server + Soft-AP + NVS
`-- locator_app/                # host-side artifacts (no server today)
    |-- SimFlow_HLD.md          # high-level design
    |-- integration_guide.md    # this file
    `-- webui/
        `-- index.html          # control page, embedded into firmware
```

## 2. Build & Flash

The firmware is built like any other ESP-IDF project from the SimFlow repo
root. The locator is the only device component required by `main/`.

```bash
# one-time
idf.py set-target esp32s3

# build / flash / monitor
idf.py build
idf.py -p COMx flash monitor
```

### Credentials

Fill in `locator_fw/config.h` before flashing:

| Macro          | Purpose                                         |
|----------------|-------------------------------------------------|
| `WIFI_SSID`    | Default STA SSID (used until user changes via Soft-AP UI) |
| `WIFI_PASSWORD`| Default STA password                            |
| `BOT_TOKEN`    | Telegram bot token from @BotFather. Leave empty to disable Telegram. |
| `CHAT_ID`      | Telegram chat id (numeric, negative for groups) |

The file is gitignored. Empty `BOT_TOKEN` / `CHAT_ID` make Telegram calls no-ops.

## 3. First Boot

1. After flash, the device tries to join `WIFI_SSID` for ~20 seconds.
2. Regardless of success, the Soft-AP comes up:
   * Default SSID `simpleBot-AP`, password `simplebot123` (changeable via UI,
     persisted in NVS).
3. Connect to the Soft-AP, browse to `http://192.168.4.1/`, and use the page
   to:
   * Update AP credentials (the AP will restart with new creds).
   * Calibrate the MPU baseline Az (keep device still).
   * Set LED color (named or raw RGB).
4. The device blinks red by default; the BOOT button (GPIO 0) cycles colors
   and double-click triggers a chip-temperature message to Telegram.

## 4. Editing the Web UI

The control page is **embedded into the firmware binary** at build time:

```cmake
EMBED_TXTFILES "locator_app/webui/index.html"
```

To change the UI, edit `locator_app/webui/index.html` and rebuild. The C
firmware references it via linker symbols:

```c
extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[]   asm("_binary_index_html_end");
```

There is **no Vite/React build step** - the page is plain HTML/JS so the
device can serve it directly without a bundler.

## 5. HTTP API (served on port 80 by the device)

| Method | Path             | Body                                  | Effect                                     |
|--------|------------------|---------------------------------------|--------------------------------------------|
| GET    | `/`              | -                                     | Returns `index.html` (embedded).           |
| GET    | `/api/status`    | -                                     | JSON: AP info, MPU baseline + temp, LED state. |
| POST   | `/api/ap`        | `{"ssid","password"}`                 | Persist AP creds to NVS; schedule restart. |
| POST   | `/api/calibrate` | -                                     | Re-baseline MPU Az; sends Telegram on success. |
| POST   | `/api/led`       | `{"color"}` or `{"r","g","b"}`        | Set LED named color or raw RGB.            |

## 6. Telegram Alerts the Device Sends

| Trigger                                            | Message |
|----------------------------------------------------|---------|
| Boot                                               | "simpleBot started! Blinking red." |
| Lift detected (`az` drop > 2.5 m/s^2, 3 s cooldown) | "Lift detected: device was picked up" |
| MPU temperature >= 60 C                            | "WARNING: MPU6050 temperature high ..." |
| MPU temperature recovered <= 55 C                  | "MPU6050 temperature recovered ..." |
| 1-minute motion window with any movement events    | "Motion summary (last 1 min): ..." |
| BOOT button single-click                           | "Color changed to <c>" / "LED turned off" |
| BOOT button double-click                           | (chip temperature - currently N/A; ESP32-S3 internal sensor not wired) |
| New MPU baseline after `/api/calibrate`            | "MPU6050 calibrated. Baseline az=..." |
