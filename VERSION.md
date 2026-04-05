# EPrices – Version History

## v1.0 — 2026-04-05

First public release.

### What EPrices is

EPrices is an ESPHome firmware for ESP32 that fetches day-ahead electricity
spot prices from the public Energy-Charts API and exposes them as Home Assistant
sensors. No API token, no cloud subscription, no external automations required
for core functionality — just an ESP32, ESPHome, and your WiFi network.

Prices are fetched for today and tomorrow in 15-minute resolution, converted
from raw €/MWh to €/kWh with your provider fee and VAT applied, and stored
persistently in ESP32 NVS flash so they survive reboots without re-fetching.

### Core features

- 15-minute and hourly resolution price arrays exposed as JSON text sensors
- Current, next, average, highest and lowest price sensors for both today and tomorrow
- Tomorrow live sensors evaluate at `now + 86400s` — showing tomorrow at the same local time
- NVS persistence — prices survive reboots without re-fetching
- Midnight bridge — tomorrow's data automatically promoted to today at 00:00
- Auto-retry logic — up to 8 HTTP fetch attempts for both today and tomorrow
- DST-safe — UNIX timestamps and binary search throughout, no hour-slot arithmetic
- Staleness detection — `Today Current Price Status` shows `Stale` if stored date mismatches today
- Full diagnostic sensor suite — NVS status, fetch attempts, API fetch times, data loaded
  times, WiFi signal, human-readable uptime
- Supports any Energy-Charts bidding zone (SI, DE-LU, AT, FR, HR, HU and more)
- Provider fee and VAT rate configurable via `secrets.yaml` — no code changes needed

### Sensor highlights

- **Today/Tomorrow JSON Hourly Prices EUR⁄kWh** — 24-value JSON arrays
- **Today/Tomorrow JSON 15-Min Prices EUR⁄kWh (P1/P2/P3)** — three 32-value JSON arrays
  covering 00:00-07:45, 08:00-15:45 and 16:00-23:45
- **Today/Tomorrow Data Loaded Time** — stamped on both NVS load and HTTP fetch
- **Today/Tomorrow Last API Fetch Time** — stamped on HTTP fetch only; `Never` if NVS only
- **Today/Tomorrow API Fetch Attempts** — HTTP-only counter, resets at midnight
- **Uptime** — human-readable: `45 s` → `5 min` → `3 h 22 min` → `12 d 4 h` → `4 months 12 d`

### Files

| File | Purpose |
|---|---|
| `eprices.yaml` | Main ESPHome configuration |
| `eprices_nvs.h` | NVS helper — save/load price arrays to ESP32 flash |
| `secrets.yaml` | Local secrets — not committed to git |
| `CHANGELOG.md` | Full sensor and entity ID reference for v1.0 |
| `README.md` | Installation guide and full sensor reference |
| `ENTSO-E-PRICES-MIGRATION.md` | Optional — migration guide from the predecessor project |