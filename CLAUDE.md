# CLAUDE.md — Indicator Fuel Finder

GPS-driven gas-station finder built on a **SenseCAP Indicator D1L**, for a 7-day
Florida road trip. Goal: a useful in-car node that is also a clean practice
project across GPS, a POI API, on-device geometry, and an LVGL UI. Meshtastic is
left untouched — this runs as separate custom firmware.

Suggested repo name: `indicator-fuel-finder`.

---

## 1. Hardware

**SenseCAP Indicator D1L** (Seeed SKU `114993532`)
- MCUs: ESP32-S3 (up to 240 MHz, 8 MB flash) + RP2040 (up to 133 MHz, 2 MB flash)
- Display: 3.95" RGB capacitive touch, 480 × 480
- Radios: SX1262 LoRa (862–930 MHz), Wi-Fi 2.4 GHz, BLE 5.0
- Storage: microSD up to 32 GB (not included)
- Buzzer: MLT-8530
- Expansion: 2× Grove ports — **both wired to the RP2040, not the ESP32-S3**:
  `Grove(IIC)` = I²C (RP2040 GPIO20 SDA / GPIO21 SCL, power-enable GPIO18 active-high)
  and `Grove(ADC)` = analog (GPIO26/27). Inter-proc UART: ESP32-S3 GPIO19/20 ↔ RP2040.
- Power: USB-C, 5 V / 1 A
- **NOT onboard:** GPS, battery, cellular

**Externals**
- **Grove – GPS (Air530Z) v1.1** (owned) — UART/NMEA, default **9600 baud**, 3.3 V OK;
  emits RMC (course-over-ground). Plugs into the **`Grove(IIC)`** port → read by the
  **RP2040** (its pins double as RP2040 UART1). See `docs/hardware/`.
- Car USB-C charger — device lives powered (no internal battery).
- Phone Wi-Fi hotspot (2.4 GHz) — the only uplink while driving.

> **Hardware details:** see [`docs/hardware/`](docs/hardware/) for the full Grove
> pinout, GPS wiring table, on-device probe (chip/flash/MAC), and source manuals.

---

## 2. Architecture (v1)

**Standalone on the Indicator.** No runtime dependency on home infra.

> **Revised v1 data path** (the original "GPS → ESP32-S3 direct" was wrong: the Grove
> ports are on the RP2040). The GPS is read by the RP2040 and forwarded to the
> ESP32-S3 over the existing inter-processor UART. **No soldering** — chosen Option A.

```
Air530Z GPS ──(NMEA/UART1 9600)──> RP2040 ──(inter-proc UART)──> ESP32-S3 ──(HTTPS over phone hotspot)──> TomTom POI Search
   (Grove(IIC) port)                  │         (GPIO19/20)          │
                                      └ enable Grove power (GPIO18)  ├── geometry (haversine + bearing), filtering
                                                                     ├── LVGL UI (list + QR)
                                                                     └── buzzer alerts (MLT-8530)
```

The data uplink is outbound-only to TomTom; nothing needs to be exposed inbound.

> **Two firmwares now:** ESP32-S3 (app) **and** RP2040 (GPS reader/forwarder). The
> RP2040 still owns Grove + buzzer + SD. Meshtastic on the ESP32-S3 stays intact.

---

## 3. Firmware stack

- **PlatformIO + Arduino framework** (developer uses VSCode + PlatformIO on Fedora 43).
- **LVGL 8.x** for UI (includes `lv_qrcode` for the QR widget).
- Libraries: `TinyGPSPlus` (NMEA parsing), `ArduinoJson` (POI response),
  `WiFiClientSecure` / `HTTPClient` (HTTPS GET).
- ESP-IDF is a possible later migration (matches factory firmware); not needed for v1.
- **RP2040 firmware (new, Option A):** Arduino-Pico (earlephilhower core). Enables
  the `Grove(IIC)` power switch (GPIO18 high), reads UART1 (GPIO20 TX / GPIO21 RX)
  at 9600, and forwards NMEA/position to the ESP32-S3 over the inter-proc UART.
  TinyGPSPlus parsing can live on either side; default to parsing on the RP2040 and
  shipping clean lat/lng/CoG.

Repo layout (two PlatformIO projects — GPS is on the RP2040, see `docs/hardware/`):

```
firmware/
  PROTOCOL.md          # RP2040 -> ESP32-S3 "FIX,..." line protocol
  rp2040/              # GPS reader: NMEA (UART1) -> FIX lines (UART0) [built, verified]
  esp32-s3/src/
    config/            # config.example.h -> config.h (git-ignored): creds, key, thresholds
    gps/               # gps_link: parse FIX lines from the RP2040
    poi_client/        # IFuelProvider interface + TomTomProvider impl
    geo/               # haversine distance, bearing, heading filter
    ui/                # LVGL list view + station detail w/ QR  [STUB: pending panel driver]
    alerts/            # proximity / low-fuel buzzer decision (buzzer is on the RP2040)
tools/rp2040-gps-probe/  # throwaway diagnostic used to confirm GPS wiring
```

---

## 4. Data provider (POI)

**Primary: TomTom POI Search** (Places / Search API).
- Freemium: 2,500 non-tile requests/day free, no credit card, commercial OK.
- Overage: POI Search $2.50 / 1,000 (we stay well inside free tier → $0 for the trip).
- **Must request and parse the address field**, not just the name.
- Filter to fuel/petrol category (confirm exact `categorySet` code — see Pending).

**Abstraction:** all provider access goes through an `IFuelProvider` interface
with a single core method, e.g. `getNearby(lat, lng) -> [Station]`, where
`Station = { brand, address, lat, lng, providerId, distance, bearing }`.
This keeps **Google Places** and **OSM Overpass** swappable without touching the
rest of the firmware.

---

## 5. Features (v1)

1. Parse GPS continuously: position + course-over-ground (NMEA RMC).
2. On **movement threshold (~3 km)** query the provider for nearby fuel stations.
3. Compute **distance (haversine)** and **bearing** to each station locally.
4. **Heading filter** — show only stations "ahead": bearing within ±70° of CoG.
   - Apply the filter **only above ~15 km/h** (CoG is noisy when slow/stopped).
   - Below the threshold, fall back to plain radial-nearest.
5. **LVGL list UI:** brand + address + distance + bearing.
6. **Station detail = QR code** (`lv_qrcode`) encoding `geo:lat,lng` (or a Maps
   URL). Passenger scans it → opens the exact station in Maps → cross-reference
   price in GasBuddy. This is the primary disambiguation UX.
7. **Proximity buzzer** (MLT-8530) when within X m of a target / in low-fuel mode.
8. Optional brand filter (e.g., only certain chains).

### Disambiguation note
Provider POI IDs (TomTom `id`, Google `place_id`) are **provider-internal** and
do **not** map to GasBuddy's catalog. The human join key is **brand + full
address + coords**. The QR is what removes the "which of the five Shells?"
ambiguity for the passenger's GasBuddy lookup.

---

## 6. Out of scope for v1 (non-goals)

- **Fuel prices on-device.** TomTom Fuel Prices is enterprise/contact-sales;
  cheap APIs (CollectAPI/Zyla) only return regional averages. Price lookup is a
  manual passenger workflow via GasBuddy on a phone.
- **MQTT / home-infra integration.** No runtime dependency on GeekLab.
- **Route-following / search-along-route.** True along-route POIs need a routing
  API + a fixed destination; deferred to a later phase. v1 = radial + heading filter.
- **Do not modify the Meshtastic firmware partition.** This is a separate custom
  firmware; Meshtastic must remain intact and recoverable.

---

## 7. Pending verification (do this before/while scaffolding)

1. ~~**Grove port wiring (KEY HARDWARE UNKNOWN).**~~ **RESOLVED 2026-06-03.** Both
   Grove ports route to the **RP2040**. `Grove(IIC)` = GPIO20/21 (+GPIO18 power),
   `Grove(ADC)` = GPIO26/27. → We flash **two firmwares** (ESP32-S3 app + RP2040
   GPS reader). See `docs/hardware/grove-ports.md`.
2. ~~**Grove GPS UART details.**~~ **RESOLVED 2026-06-03 (empirically).** Air530Z =
   UART/NMEA, **9600 baud**, `$GNRMC` present, `ANTENNA OK`. On `Grove(IIC)` it lands
   on **RP2040 UART1** (GPIO20 TX / GPIO21 RX) with a straight Grove cable, no
   crossover. **GPIO18 active-high power switch confirmed.** Physical socket: the
   **RIGHT** one with the device back facing you (`Grove(ADC)` = left = unusable for
   GPS; sockets are not labeled on the unit). In arduino-pico, UART1 = **`Serial2`**.
   Verified with `tools/rp2040-gps-probe/`. See `docs/hardware/gps-air530z-wiring.md`.
3. **TomTom request shape.** Confirm the fuel/petrol `categorySet`, the field
   mask / returned address fields, result limit, and radius parameter.
4. **Display driver config.** Confirm the RGB panel controller + pin map for the
   Indicator under the PlatformIO Arduino setup (may need Seeed's board
   definition / reference repo).
5. **LVGL version pin.** Lock the LVGL major version and confirm `lv_qrcode`
   availability for that version.
6. **Phone hotspot band.** Confirm the phone hotspot can run on 2.4 GHz (device
   has no 5 GHz radio).
7. **TomTom pricing change effective 2026-07-01** — re-check terms if this becomes
   a permanent project.

---

## 8. Decision log (the "why")

- **Standalone over an MQTT pipeline (v1):** fewer moving parts, no dependency on
  home infra reachability while driving. MQTT/Grafana breadcrumb is a nice phase-2
  flourish, not a requirement.
- **TomTom POI Search over Google Places / Overpass:** lowest onboarding friction
  (no credit card, generous free daily tier, clean JSON). Google forces billing
  enablement even on the free tier; Overpass means fighting Overpass QL and
  inconsistent OSM tagging. Kept behind `IFuelProvider` so any of them is swappable.
- **No prices in v1:** all the cost/coverage/enterprise pain lives in price data;
  the core feature (nearest/ahead + alert) does not need it. GasBuddy on a phone
  covers price, and the QR makes that lookup trivial.
- **Radial + heading filter over route-search:** ~90% of the "ahead vs behind"
  value at zero extra API cost and minimal code.
- **GPS on the RP2040 via `Grove(IIC)` (Option A), not soldered to the ESP32-S3:**
  the only UART-capable Grove pins are the RP2040's, and the user ruled out
  soldering. Cost is a second (RP2040) firmware + an inter-proc UART hop; benefit is
  a clean plug-in install with no hardware mods. The straight-cable mapping happens
  to land GPS TX on UART1 RX, so no crossover is needed. (2026-06-03)

---

## 9. Phase 2+ (post-trip, optional)

- MQTT telemetry breadcrumb → HiveMQ → Grafana dashboard.
- Regional average price overlay via CollectAPI/Zyla (clearly labeled as average,
  not per-pump).
- "Last station before a long gap" alert using remaining range.
- True search-along-route once a destination/route is in play.

---

## 10. Conventions

- **English** for all code, comments, commits, and docs (project standard).
- Secrets (Wi-Fi creds, TomTom API key) in `config/` and git-ignored; never commit keys.
- Dev environment: PlatformIO in VSCode on Fedora 43.
