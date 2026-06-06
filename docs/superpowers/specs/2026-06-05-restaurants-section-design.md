# Design — "Gas + Eat" Finder (restaurants section)

_Date: 2026-06-05. Branch: `feature/restaurants-foursquare`._

Replaces the per-station QR detail (judged not useful in practice) with a second
on-screen section: **top-rated restaurants ahead**, below the existing fuel list.
Gas keeps its verified TomTom path untouched; restaurants come from a new
Foursquare provider behind the same provider abstraction.

---

## 1. Scope

**In:**
- Remove the QR code and the station detail screen (`showDetail` / `lv_qrcode`).
- Add a restaurants section under the fuel section on the same 480×480 screen.
- Add a **Foursquare** restaurant provider (Premium tier — returns `rating`).
- Generalize the provider abstraction (`Station`→`Place`, `IFuelProvider`→`IPlaceProvider`)
  so fuel (TomTom) and food (Foursquare) share one interface and the geo layer.

**Out (unchanged / non-goals):**
- Gas data source — stays TomTom, behavior-identical.
- Touch (still optional / not required for this feature).
- Prices, route-following, MQTT — same v1 non-goals as `CLAUDE.md §6`.

---

## 2. UI (LVGL) — 50/50 split, 2 + 2 with detail

```
⛽ GAS AHEAD                         [gz logo]
 Shell                    1.2mi N
  123 Main St, Tampa
 Chevron                  2.4mi NE
  456 Dale Mabry Hwy
─────────────────────────────────────────────
🍴 EAT  (★≥8.6)
 Longhorn Steakhouse      2.1mi  ★8.9
  Steakhouse · American
 Pasha                    4.0mi  ★9.1
  Mediterranean · Greek
```

- **Top half — fuel:** 2 nearest-ahead stations. Row = brand + distance(mi) + N/S/E/W
  on line 1; full address in grey on line 2. Nearest highlighted green. (Same row
  style as today, count reduced from 5 to 2.)
- **Bottom half — food:** 2 nearest-ahead qualifying restaurants. Row = name +
  distance(mi) + `★rating` on line 1; cuisine/category in grey on line 2. Nearest
  highlighted green.
- **Logo** (geekendzone.com) stays top-right.
- **Empty states:** fuel → `No fuel ahead — searching…`; food → `No top-rated spots nearby`.
- Distances in **miles**, 1 decimal, drop trailing `.0` (existing convention).

`ui.h`: replace `showStations(...)` + `showDetail(...)` with a single
`showScreen(const std::vector<Place>& fuel, const std::vector<Place>& food)`.
Delete the QR/detail code paths.

---

## 3. Data — Foursquare provider (restaurants)

- **Request:** Foursquare Places Search around the GPS lat/lng, sorted by distance,
  constrained to restaurant cuisine categories, `US`.
- **Fields requested:** name, address, lat/lng, distance, `categories`, **`rating`**.
  `rating` is a **Premium** field — requesting it makes the call billable
  (~$18.75 / 1,000). This is accepted (est. ~$10–15 for the 7-day trip).
- **Cuisine filter:** include Mediterranean / Greek / Peruvian / American-steakhouse
  style; **exclude** fast-food and Mexican. Done via Foursquare category IDs in the
  request where possible; exact IDs confirmed empirically (same approach used to
  confirm TomTom `7311`).
- **Quality filter:** keep `rating ≥ RESTAURANT_MIN_RATING` (default **8.6**/10 ≈
  4.3/5), filtered client-side. Restaurants with no `rating` are dropped.
- **Ranking / heading:** reuse the existing geo layer — compute bearing locally and
  apply the same **heading filter (±70° of course-over-ground, only above ~15 km/h;
  nearest-radial below that)** as fuel, so "EAT AHEAD" is literal. Show the **2
  nearest** that survive the filters.
- **Cadence / cost:** the food query fires on the **same ~3 km movement trigger** as
  the fuel query. A configurable larger threshold for food (`RESTAURANT_MOVE_KM`,
  default = fuel threshold) is available to cut cost if desired.

---

## 4. Architecture (code)

Generalize the abstraction `CLAUDE.md §4` already called for, without changing fuel
behavior:

- **`Place`** struct (was `Station`): existing fields (brand/name, address, lat, lng,
  providerId, distanceM, bearingDeg) **plus** `float rating = -1` and
  `String category` — left at defaults for fuel.
- **`IPlaceProvider`** interface (was `IFuelProvider`): same
  `getNearby(lat, lng, out) -> bool`. `TomTomProvider` re-typed to it,
  behavior-identical. New **`FoursquareProvider`** implements it for food.
- **`geo/`** (haversine, bearing, heading filter) operates on `Place` generically →
  reused as-is for both lists.
- **App loop:** on a movement trigger, query both providers, run each result set
  through the geo filter, then `ui::showScreen(fuel, food)`.
- **Trade-off:** this touches the on-device-verified fuel path. The change is a
  rename + added optional fields with **no behavior change**; re-verify fuel on the
  device after the refactor.

---

## 5. Config (`config.h`, git-ignored; mirror in `config.example.h`)

| Key | Default | Use |
| --- | --- | --- |
| `FOURSQUARE_API_KEY` | — | Foursquare service key (scope + spend cap; rotate before/after trip) |
| `RESTAURANT_MIN_RATING` | `8.6` | Min Foursquare rating (0–10) to show |
| `RESTAURANT_CATEGORIES` | (cuisine IDs) | Foursquare category IDs to include |
| `RESTAURANT_RADIUS_M` | `10000` | Search radius (m) |
| `RESTAURANT_LIMIT` | small (e.g. 20) | Max raw results to fetch before filtering |
| `RESTAURANT_MOVE_KM` | = fuel threshold | Movement before re-querying food (cost knob) |

---

## 6. Verify during implementation (do not assume)

- Foursquare current **host/endpoint** and **auth header** (Service Key / `Authorization`)
  — the platform changed in 2024–25.
- Exact Foursquare **category IDs** for the wanted cuisines, and that `rating` is
  actually populated (may be absent for venues with few check-ins).
- Re-verify the **fuel path on-device** after the `Place`/`IPlaceProvider` refactor.
- 🔑 Second API key lands in ESP32 flash (extractable) — same handling as the TomTom
  key: Search-only scope, spend cap, never commit, rotate.

---

## 7. Done = (acceptance)

- On-device: screen shows up to 2 nearest-ahead fuel stations (top) and up to 2
  nearest-ahead restaurants with `rating ≥ 8.6` and an allowed cuisine (bottom),
  no QR, logo present, distances in miles.
- Fuel behavior unchanged vs. before the refactor (re-verified on device).
- Foursquare key/thresholds/categories all in git-ignored `config.h`; example file updated.
