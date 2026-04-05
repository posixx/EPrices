# Migrating from Entso-E Prices to EPrices

This document explains the rationale behind EPrices and documents all changes
relative to the last public release of the predecessor project,
**entso-e-prices v4.3.1**.

It is intended for users who ran entso-e-prices v4.3.1 and want to migrate
their Home Assistant automations, dashboards, and energy configurations to EPrices.

---

## Why EPrices was created

The original entso-e-prices project fetched electricity prices from the
**ENTSOE Transparency Platform XML API**. That API requires a private token,
has rate limits, and returns data in a complex XML format that required
significant parsing logic and an external helper file (`entsoe_http_idf.h`).
Additionally, all automation logic (midnight bridge, retry scheduling, NVS
management) was split across external HA automations, making the system fragile
and difficult to maintain.

The goal of EPrices was to:

1. **Eliminate the need for a private API token** — the Energy-Charts API
   (Fraunhofer ISE) is public, free, and returns clean JSON. No registration,
   no token rotation, no rate limit concerns.

2. **Move all core logic into the device** — midnight bridge, retry scheduling,
   NVS persistence, and boot recovery all run on the ESP32 itself. No external
   HA automations are required for any core functionality.

3. **Simplify the codebase** — the two external helper files
   (`entsoe_http_idf.h` and `entsoe_storage_v2.h`) were eliminated. All logic
   is now either inline in `eprices.yaml` or in the single helper `eprices_nvs.h`.

4. **Improve sensor clarity and user experience** — sensor names were
   systematically restructured with Today/Tomorrow prefixes, redundant words
   removed, and diagnostic sensors clearly separated from operational ones.

5. **Make the system DST-safe** — the original slot arithmetic
   `(hour × 4) + (min / 15)` breaks on DST transition days. EPrices uses
   UNIX timestamps and binary search throughout, making it fully DST-safe.

---

## Summary of all changes from entso-e-prices v4.3.1 to EPrices v1.0

### API

| Item | entso-e-prices v4.3.1 | EPrices v1.0 |
|---|---|---|
| Data source | ENTSOE Transparency Platform XML | Energy-Charts JSON (Fraunhofer ISE) |
| Authentication | Private API token required | No token — public API |
| Data format | XML | JSON |
| Price resolution | 15-minute | 15-minute |
| Base price unit | €/MWh | €/MWh (converted to €/kWh in firmware) |

### Project structure

| Item | entso-e-prices v4.3.1 | EPrices v1.0 |
|---|---|---|
| ESPHome node name | `entso-e-prices` | `eprices` |
| Friendly name | `Entso-E Prices` | `EPrices` |
| Helper files | `entsoe_http_idf.h` + `entsoe_storage_v2.h` | `eprices_nvs.h` only |
| NVS namespace | `entsoe2` | `eprices` |
| Secret key prefix | `entsoe_` | `eprices_` |
| Provider fee / VAT | Hardcoded in lambdas | Configurable via `secrets.yaml` |
| Core automation logic | Split across external HA automations | Fully on-device |

### NVS storage

| Item | entso-e-prices v4.3.1 | EPrices v1.0 |
|---|---|---|
| Storage format | Fixed 96-slot float blobs | Dynamic timestamp + price arrays |
| DST handling | Hour-slot arithmetic `(h×4)+(m/15)` — breaks on DST | UNIX timestamp binary search — DST-safe |
| Boot recovery | Via external HA automation | On-device `boot_recovery_today/tomorrow_script` |
| Midnight bridge | Via external HA automation | On-device `midnight_bridge_promotion` script |

### Fetch scheduling and retry logic

| Item | entso-e-prices v4.3.1 | EPrices v1.0 |
|---|---|---|
| Today fetch trigger | External HA automation | On-device auto-retry (up to 8 attempts) |
| Tomorrow fetch schedule | External HA automation | On-device 13:25 → 13:55 → 14:55 … 19:55 |
| Manual force update | Button | Button (unchanged) |
| Midnight bridge | External HA automation at 00:00 | On-device `on_time: 00:00:00` |

### Sensor naming

All sensors were renamed with a systematic **Today / Tomorrow** prefix.
The words "electricity" and "energy" were removed from all sensor names.
"Next Day" was replaced with "Tomorrow" throughout.

#### Numeric sensors

| entso-e-prices v4.3.1 name | EPrices v1.0 name |
|---|---|
| `Current Electricity Price` | `Today Current Price` |
| `Next Electricity Price` | `Today Next Price` |
| `Average Electricity Price Today` | `Today Average Price` |
| `Highest Electricity Price Today` | `Today Highest Price` |
| `Lowest Electricity Price Today` | `Today Lowest Price` |
| `Current Hourly Electricity Price` | `Today Current Hourly Price` |
| `Next Hourly Electricity Price` | `Today Next Hourly Price` |
| `Highest Hourly Electricity Price Today` | `Today Highest Hourly Price` |
| `Lowest Hourly Electricity Price Today` | `Today Lowest Hourly Price` |
| `Current Max Hourly Price Percentage` | `Today Current Max Hourly Price Percentage` |
| `Daily Price Update Attempts` | `Today API Fetch Attempts` *(moved to text_sensor)* |
| `Today Entry Count` | `Today Entry Count` *(moved to text_sensor)* |
| `Next Day Current Electricity Price` | `Tomorrow Current Price` |
| `Next Day Next Electricity Price` | `Tomorrow Next Price` |
| `Average Electricity Price Tomorrow` | `Tomorrow Average Price` |
| `Highest Electricity Price Tomorrow` | `Tomorrow Highest Price` |
| `Lowest Electricity Price Tomorrow` | `Tomorrow Lowest Price` |
| `Current Hourly Electricity Price Tomorrow` | `Tomorrow Current Hourly Price` |
| `Next Hourly Electricity Price Tomorrow` | `Tomorrow Next Hourly Price` |
| `Highest Hourly Electricity Price Tomorrow` | `Tomorrow Highest Hourly Price` |
| `Lowest Hourly Electricity Price Tomorrow` | `Tomorrow Lowest Hourly Price` |
| `Next Day Current Max Hourly Price Percentage` | `Tomorrow Current Max Hourly Price Percentage` |
| `Next Day Price Update Attempts` | `Tomorrow API Fetch Attempts` *(moved to text_sensor)* |
| `Tomorrow Entry Count` | `Tomorrow Entry Count` *(moved to text_sensor)* |

#### Text sensors

| entso-e-prices v4.3.1 name | EPrices v1.0 name |
|---|---|
| `ENTSO-E Hourly Prices EUR⁄kWh JSON` | `Today JSON Hourly Prices EUR⁄kWh` |
| `ENTSO-E 15-Min Prices EUR⁄kWh JSON (P1 00:00-07:45)` | `Today JSON 15-Min Prices EUR⁄kWh (P1 00:00-07:45)` |
| `ENTSO-E 15-Min Prices EUR⁄kWh JSON (P2 08:00-15:45)` | `Today JSON 15-Min Prices EUR⁄kWh (P2 08:00-15:45)` |
| `ENTSO-E 15-Min Prices EUR⁄kWh JSON (P3 16:00-23:45)` | `Today JSON 15-Min Prices EUR⁄kWh (P3 16:00-23:45)` |
| `Time Of Highest Energy Price Today` | `Today Highest Price Time` |
| `Time Of Lowest Energy Price Today` | `Today Lowest Price Time` |
| `Time Of Highest Hourly Energy Price Today` | `Today Highest Hourly Price Time` |
| `Time Of Lowest Hourly Energy Price Today` | `Today Lowest Hourly Price Time` |
| `Price Update Status` | `Today Price Update Status` |
| `Last Price Update Time` | `Today Data Loaded Time` |
| `Price Update Status Message` | `Today Price Update Status Message` |
| `Current Price Status` | `Today Current Price Status` |
| `ENTSO-E Next Day Hourly Prices EUR⁄kWh JSON` | `Tomorrow JSON Hourly Prices EUR⁄kWh` |
| `ENTSO-E Next Day 15-Min Prices EUR⁄kWh JSON (P1 00:00-07:45)` | `Tomorrow JSON 15-Min Prices EUR⁄kWh (P1 00:00-07:45)` |
| `ENTSO-E Next Day 15-Min Prices EUR⁄kWh JSON (P2 08:00-15:45)` | `Tomorrow JSON 15-Min Prices EUR⁄kWh (P2 08:00-15:45)` |
| `ENTSO-E Next Day 15-Min Prices EUR⁄kWh JSON (P3 16:00-23:45)` | `Tomorrow JSON 15-Min Prices EUR⁄kWh (P3 16:00-23:45)` |
| `Time Of Highest Energy Price Tomorrow` | `Tomorrow Highest Price Time` |
| `Time Of Lowest Energy Price Tomorrow` | `Tomorrow Lowest Price Time` |
| `Time Of Highest Hourly Energy Price Tomorrow` | `Tomorrow Highest Hourly Price Time` |
| `Time Of Lowest Hourly Energy Price Tomorrow` | `Tomorrow Lowest Hourly Price Time` |
| `Next Day Price Update Status` | `Tomorrow Price Update Status` |
| `Next Day Last Price Update Time` | `Tomorrow Data Loaded Time` |
| `Next Day Price Update Status Message` | `Tomorrow Price Update Status Message` |
| `Next Day Current Price Status` | `Tomorrow Current Price Status` |
| `ENTSO-E Last Reboot` | `Last Reboot` |
| `Entso-E Today NVS Status` | `Today NVS Status` |
| `Entso-E Tomorrow NVS Status` | `Tomorrow NVS Status` |
| `ENTSO-E Last Update Source` | `Last Update Source` |
| `Today Data Date` | `Today Data Date` *(unchanged)* |
| `Tomorrow Data Date` | `Tomorrow Data Date` *(unchanged)* |

#### Buttons

| entso-e-prices v4.3.1 name | EPrices v1.0 name |
|---|---|
| `Entso-E Force Update` | `Force Today's Update` |
| `Entso-E Force Next Day Update` | `Force Tomorrow's Update` |
| `Entso-E Reboot Device` | `Reboot Device` |

### New sensors in EPrices v1.0 (no equivalent in v4.3.1)

| Sensor | Purpose |
|---|---|
| `Today Last API Fetch Time` | Timestamp of last successful HTTP fetch for today |
| `Tomorrow Last API Fetch Time` | Timestamp of last successful HTTP fetch for tomorrow |
| `WiFi Signal` | RSSI in dBm |
| `Uptime` | Human-readable uptime string |

### Sensor output changes

| Sensor | v4.3.1 output | EPrices v1.0 output |
|---|---|---|
| `Last Reboot` | `Last reboot: 2026-04-04 20:10:10` | `2026-04-04 20:10:10` |
| `Today NVS Status` | `Today NVS: Stored 96 pts for 2026-04-04` | `Stored 96 pts for 2026-04-04` |
| `Tomorrow NVS Status` | `Tomorrow NVS: Stored 96 pts for 2026-04-05` | `Stored 96 pts for 2026-04-05` |
| `Today Entry Count` | `96.0` | `96` |
| `Tomorrow Entry Count` | `96.0` | `96` |
| `Today API Fetch Attempts` | `0.0` | `0` |
| `Tomorrow API Fetch Attempts` | `0.0` | `0` |
| `Today Current Price Status` | `Valid` / `Missing` | `Valid` / `Missing` / `Stale` |
| `Today Data Loaded Time` | Stamped on HTTP fetch only | Stamped on NVS load and HTTP fetch |
| `Tomorrow Data Loaded Time` | `Never` after NVS boot load | Stamped on NVS load and HTTP fetch |

### HA entity ID changes

All entity IDs changed due to the node name change from `entso-e-prices` to `eprices`.

**Find and replace in all your automations and dashboards:**
```
sensor.entso_e_prices_  →  sensor.eprices_
button.entso_e_prices_  →  button.eprices_
```

Then apply the individual sensor name slug changes from the tables above.

### NVS data

The NVS namespace changed from `entsoe2` to `eprices`. On first boot after
flashing EPrices v1.0 the device will not find any stored data and will
trigger a fresh HTTP fetch automatically. This is expected and safe.
No manual NVS erase is required.

---

## Migration checklist

- [ ] Flash `eprices.yaml` to the device
- [ ] Update `secrets.yaml` — rename all `entsoe_` keys to `eprices_`; add `eprices_prov_fee` and `eprices_vat_rate`
- [ ] In all HA automations: replace `entso_e_prices_` with `eprices_` in all entity IDs
- [ ] Apply individual sensor name slug renames from the tables above
- [ ] Remove any external HA automations that handled midnight bridge, boot recovery, or fetch scheduling — these are now all on-device
- [ ] Verify the device fetches fresh data on first boot (check `Today NVS Status` and `Today Price Update Status Message`)
- [ ] Update any HA dashboard cards referencing old entity IDs