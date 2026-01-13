# ESP32-WebRadio

> This is the entire PlatformIO project (updated).
>
> Remember to set your SPIFFS (LittleFS) partition to 2Mb/2Mb and then upload the data partition FIRST. Then upload your project - the data in SPIFFS/LittleFS partition will remain unaffected.
>
> You MUST calibrate the ILI9341 TFT Touch Screen. It will generate the calibration file (example in the .DATA folder). I just copied the bytes out of it (by displaying them on screen) so they would not get overwritten if I uploaded the SPIFFS partition again. This will be refined going forward.
>
> - Ralph S. Bacon (original project notes)

## Why
This is a hardware-first ESP32 web radio with a touch UI that can evolve into a Home Assistant friendly player.

## Hardware
- LilyGO TTGO T8 v1.7.1 (ESP32, 4MB flash, 8MB PSRAM)
- ILI9341 TFT + XPT2046 touch (TFT_eSPI)
- VS1053 MP3 decoder module

## Software / Capabilities
- PlatformIO + Arduino framework
- LVGL v9 overlay on top of the existing TFT UI
- LittleFS data partition for stations and calibration
- Station metadata parsing (track, artist, album) shown in LVGL panel

## Build and Flash
- Build: `pio run -e ttgo_t8_v1_7_1`
- Upload data (LittleFS): `pio run -e ttgo_t8_v1_7_1 -t uploadfs`
- Upload firmware: `pio run -e ttgo_t8_v1_7_1 -t upload`
- Simulator (macOS/Linux): `pio run -e sim` then `pio run -e sim -t exec`

## Configuration
- WiFi: copy `include/secrets.h.example` to `include/secrets.h` and fill in credentials.
- Stations: edit `.data/stations.xml` then upload the LittleFS partition.

## UI Status
- LVGL panel shows track, artist, and album.
- A right-side square displays a genre-specific icon (currently drawn with LVGL primitives).

## Icons (planned)
- Place icons under `.data/icons/` so they can be updated without recompiling.
- Target size: 60x60 or 62x62 PNG with transparent background.
- Naming: `genre_rock.png`, `genre_metal.png`, `genre_classical.png`, `genre_jazz.png`, `genre_news.png`, etc.
- We'll wire PNG decoding or convert icons to LVGL C arrays in milestone-3.

## Milestones
- Milestone 2: LVGL layout + simulator working.
- Milestone 3: refine layout, add icon/image assets, and polish the UI.
