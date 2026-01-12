# ESP32 Better Internet Radio

## Source of Truth
- Build config: `platformio.ini` (`[env:stable]`).
- Core code: `src/main.cpp`, `src/main.h`.
- Helpers: `include/tftHelpers.h`, `include/wifiHelpers.h`, `include/taskHelper.h`, `include/bitmapHelper.h`.
- Data files: `data/Intro.mp3`, `data/WiFiSecrets.txt` (legacy), `data/TouchCalData1.txt`, `data/MuteIconOn.bmp`, `data/MuteIconOff.bmp`.
- Vendor libs in `lib/`: `TFT_eSPI-2.2.23`, `ESP_VS1053_Library`, `Arduino-Log`.

## Current Behavior (Code Flow)
- `setup()` initializes Serial, PSRAM-backed ring buffer, GPIOs, SPI, TFT (with touch calibration), LittleFS, VS1053, and plays `Intro.mp3`.
- WiFi SSID/password read from `include/secrets.h`, then WiFi connect; last station + brightness restored from Preferences.
- Station connect negotiates ICY metadata, handles redirects, and sets metadata interval.
- Audio playback runs in a dedicated task that drains the ring buffer into the VS1053.
- `loop()` feeds the ring buffer, reads metadata (updates track info), draws buffer %, handles station change, mute, and brightness buttons.

## Hardware Assumptions
- VS1053 pins: `VS1053_CS=32`, `VS1053_DCS=33`, `VS1053_DREQ=35`.
- ICY metadata jumper: `ICYDATAPIN=36` (input-only pin).
- Station change button: `stnChangePin=13`, TFT touch IRQ: `tftTouchedPin=15`.
- TFT pin map + backlight pin live in `lib/TFT_eSPI-2.2.23/User_Setup.h`.

## Backlog / TODOs (Not Yet Implemented)
- [ ] Gate Serial logging behind a DEBUG flag (main.cpp TODO, ToDo.h #6).
- [ ] Move station change button to an interrupt or add stronger debounce (main.cpp TODO).
- [ ] Display WiFi / LittleFS / stream errors on TFT (main.cpp TODO, ToDo.h #7).
- [ ] Investigate `client.read()` returning -1 / remote stream stalls (main.cpp TODO).
- [ ] Add HTTP response validation + retries; remove station-4 redirect override (main.cpp TODOs).
- [ ] Persist redirected station host/path to Preferences or LittleFS (main.cpp TODO).
- [ ] Convert text buttons to bitmap assets and expose button coordinates (tftHelpers.h TODOs).
- [ ] Replace buffer-level magic numbers with named constants (tftHelpers.h TODO).
- [ ] Display bitrate, SSID/RSSI, and connection progress on TFT (ToDo.h #3/#15/#18).
- [ ] Split Artist/Track into separate lines with word-safe wrapping (ToDo.h #4/#5).
- [ ] Store stations in LittleFS + optional web editor (ToDo.h #8).
- [ ] Mute until buffer stable after station change (ToDo.h #10).
- [ ] Explore Bluetooth audio input (ToDo.h #11).

## Milestones (Recommended)
1. Baseline compile + smoke test on current hardware (no functional changes).
2. Decide LVGL v9 display driver (TFT_eSPI vs alternative), integrate LVGL, verify basic display + touch input.
3. Port current UI to LVGL (station name, track text, buffer %, buttons).
4. Robustness pass (error display, reconnect logic, HTTP status checks, remove redirect hack, debug gating).
5. Enhancements (bitrate/SSID display, metadata formatting, station list management).

## Notes
- Legacy/alternate code: `src/YouTube version of main.cppx` is not compiled by PlatformIO.
