# TomTom Search API — Integration Reference (Fuel Finder)

What the firmware needs from TomTom, and nothing else. Product = **Search API**
(Places APIs family). Endpoint = **Nearby Search** (classic `/search/2/`).

> **Security:** never commit the real key. Store it as `TOMTOM_API_KEY` in
> `config/` (git-ignored) and inject at build/runtime. The key is embedded in
> ESP32 flash (extractable), so scope it to **Search API only** and set a spend
> cap. Rotate if leaked.

---

## 1. Auth

API key passed as the `key` query parameter on every request.

```
key=${TOMTOM_API_KEY}
```

No OAuth, no header auth needed for self-service Search.

---

## 2. Endpoint

**Nearby Search** — returns POIs around a lat/lon point, filterable by category.

```
GET https://api.tomtom.com/search/2/nearbySearch/.json
```

> Use the **classic** `/search/2/` path, not the `/maps/orbis/places/` (Orbis)
> variant. Orbis is the newer preview platform; classic matches the documented
> self-service pricing.

### Example request

```
https://api.tomtom.com/search/2/nearbySearch/.json
  ?key=${TOMTOM_API_KEY}
  &lat=27.9506
  &lon=-82.4572
  &radius=10000
  &categorySet=7311
  &limit=10
  &countrySet=US
```

```bash
curl 'https://api.tomtom.com/search/2/nearbySearch/.json?key=${TOMTOM_API_KEY}&lat=27.9506&lon=-82.4572&radius=10000&categorySet=7311&limit=10&countrySet=US'
```

---

## 3. Parameters we use

| Param         | Required | Value / notes                                                        |
| ------------- | -------- | -------------------------------------------------------------------- |
| `key`         | yes      | `${TOMTOM_API_KEY}`                                                   |
| `lat`         | yes      | Current latitude from the Grove GPS                                  |
| `lon`         | yes      | Current longitude from the Grove GPS                                 |
| `categorySet` | —        | `7311` = Petrol/Gasoline Station (confirm on Supported Category Codes) |
| `radius`      | —        | Search radius in **meters** (e.g. `10000` = 10 km)                  |
| `limit`       | —        | Max results (up to 100; keep small, e.g. 10, for memory)            |
| `countrySet`  | —        | `US` to constrain results                                            |
| `brandSet`    | —        | Optional brand filter (e.g. `Shell,Chevron`)                        |

> **Category code:** `7311` is the petrol-station top-level category. Verify the
> full list at TomTom's *Supported Category Codes* page before relying on it.

---

## 4. Response fields we parse

Results come back under `results[]`. For each station we care about:

| JSON path                         | Use                                                    |
| --------------------------------- | ------------------------------------------------------ |
| `results[].poi.name`              | Display name                                           |
| `results[].poi.brands[].name`     | Brand (e.g. "Shell") — for disambiguation / filter     |
| `results[].address.freeformAddress` | Full human-readable address (the GasBuddy join key)  |
| `results[].address.streetName`    | Street (for the QR / display)                          |
| `results[].address.municipality`  | City                                                   |
| `results[].position.lat`          | Station latitude (for bearing + QR `geo:` link)        |
| `results[].position.lon`          | Station longitude                                      |
| `results[].dist`                  | **Distance in meters** from the search point — precomputed by TomTom |
| `results[].id`                    | TomTom POI id (provider-internal; does NOT map to GasBuddy) |

### Illustrative response shape (abbreviated)

```json
{
  "summary": { "numResults": 10, "totalResults": 23 },
  "results": [
    {
      "type": "POI",
      "id": "g6JhMTIzNDU2Nzg5",
      "dist": 842.3,
      "poi": {
        "name": "Shell",
        "brands": [ { "name": "Shell" } ],
        "categories": [ "petrol station" ]
      },
      "address": {
        "streetName": "N Dale Mabry Hwy",
        "municipality": "Tampa",
        "countrySubdivision": "FL",
        "postalCode": "33614",
        "freeformAddress": "1234 N Dale Mabry Hwy, Tampa, FL 33614"
      },
      "position": { "lat": 27.9612, "lon": -82.5051 }
    }
  ]
}
```

> Field names/structure reflect the documented schema; values are illustrative.

---

## 5. On-device logic notes

- **Distance:** use `dist` directly for radial distance — no haversine needed for
  that. You still compute **bearing** locally for the heading filter.
- **Heading filter:** keep stations within ±70° of GPS course-over-ground, applied
  only above ~15 km/h; below that, show plain nearest by `dist`.
- **QR (passenger → GasBuddy):** build `geo:${lat},${lon}` (or a Maps URL) from
  `position` and render with `lv_qrcode`.

---

## 6. Cost & limits

- Freemium: **2,500 non-tile requests/day free**, no credit card.
- Overage: POI / Search **$2.50 per 1,000** requests.
- With a ~3 km movement-threshold trigger, expected usage is well under the free
  tier → **$0** for the trip.
- **QPS:** TomTom default rate limits range ~5–50 requests/sec per API. Not a
  concern at our cadence, but don't poll in a tight loop — gate on movement +
  a minimum interval.
- TomTom pricing changes effective **2026-07-01**; re-check if this becomes permanent.

---

## 7. ESP32 integration checklist

- Transport: HTTPS (port 443) via `WiFiClientSecure` + `HTTPClient`.
  - Simplest: `client.setInsecure()` (skips cert validation).
  - More secure: bundle the appropriate root CA and validate.
- Build the query string with the params above (URL-encode `brandSet` if used).
- Parse with **ArduinoJson** using a filter document to extract only the fields in
  §4 — keeps heap usage low on the ESP32-S3.
- Key handling: read `TOMTOM_API_KEY` from `config/` (git-ignored); never hardcode.

---

*Pairs with `CLAUDE.md` (project-level context). This file resolves the TomTom
portion of CLAUDE.md §7 pending item #3.*
