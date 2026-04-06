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

---

### Sensor naming

All sensors were renamed with a systematic **Today / Tomorrow** prefix.
The words "electricity" and "energy" were removed from all sensor names.
"Next Day" was replaced with "Tomorrow" throughout.

> **Note on entity ID slugs:** HA derives entity IDs by lower-casing the
> sensor name and replacing spaces and special characters with underscores.
> The `⁄` character in JSON sensor names may have been rendered differently
> by older ESPHome versions — verify those specific IDs against your actual
> HA instance if needed.

---

#### Numeric sensors

| entso-e-prices v4.3.1 name | v4.3.1 entity ID | EPrices v1.2 name | EPrices v1.2 entity ID |
|---|---|---|---|
| `Current Electricity Price` | `sensor.entso_e_prices_current_electricity_price` | `Today Current Price` | `sensor.eprices_today_current_price` |
| `Next Electricity Price` | `sensor.entso_e_prices_next_electricity_price` | `Today Next Price` | `sensor.eprices_today_next_price` |
| `Average Electricity Price Today` | `sensor.entso_e_prices_average_electricity_price_today` | `Today Average Price` | `sensor.eprices_today_average_price` |
| `Highest Electricity Price Today` | `sensor.entso_e_prices_highest_electricity_price_today` | `Today Highest Price` | `sensor.eprices_today_highest_price` |
| `Lowest Electricity Price Today` | `sensor.entso_e_prices_lowest_electricity_price_today` | `Today Lowest Price` | `sensor.eprices_today_lowest_price` |
| `Current Hourly Electricity Price` | `sensor.entso_e_prices_current_hourly_electricity_price` | `Today Current Hourly Price` | `sensor.eprices_today_current_hourly_price` |
| `Next Hourly Electricity Price` | `sensor.entso_e_prices_next_hourly_electricity_price` | `Today Next Hourly Price` | `sensor.eprices_today_next_hourly_price` |
| `Highest Hourly Electricity Price Today` | `sensor.entso_e_prices_highest_hourly_electricity_price_today` | `Today Highest Hourly Price` | `sensor.eprices_today_highest_hourly_price` |
| `Lowest Hourly Electricity Price Today` | `sensor.entso_e_prices_lowest_hourly_electricity_price_today` | `Today Lowest Hourly Price` | `sensor.eprices_today_lowest_hourly_price` |
| `Current Max Hourly Price Percentage` | `sensor.entso_e_prices_current_max_hourly_price_percentage` | `Today Current Max Hourly Price Percentage` | `sensor.eprices_today_current_max_hourly_price_percentage` |
| `Daily Price Update Attempts` | `sensor.entso_e_prices_daily_price_update_attempts` | `Today API Fetch Attempts` *(moved to text_sensor)* | `sensor.eprices_today_api_fetch_attempts` |
| `Today Entry Count` | `sensor.entso_e_prices_today_entry_count` | `Today Entry Count` *(moved to text_sensor)* | `sensor.eprices_today_entry_count` |
| `Next Day Current Electricity Price` | `sensor.entso_e_prices_next_day_current_electricity_price` | `Tomorrow Current Price` | `sensor.eprices_tomorrow_current_price` |
| `Next Day Next Electricity Price` | `sensor.entso_e_prices_next_day_next_electricity_price` | `Tomorrow Next Price` | `sensor.eprices_tomorrow_next_price` |
| `Average Electricity Price Tomorrow` | `sensor.entso_e_prices_average_electricity_price_tomorrow` | `Tomorrow Average Price` | `sensor.eprices_tomorrow_average_price` |
| `Highest Electricity Price Tomorrow` | `sensor.entso_e_prices_highest_electricity_price_tomorrow` | `Tomorrow Highest Price` | `sensor.eprices_tomorrow_highest_price` |
| `Lowest Electricity Price Tomorrow` | `sensor.entso_e_prices_lowest_electricity_price_tomorrow` | `Tomorrow Lowest Price` | `sensor.eprices_tomorrow_lowest_price` |
| `Current Hourly Electricity Price Tomorrow` | `sensor.entso_e_prices_current_hourly_electricity_price_tomorrow` | `Tomorrow Current Hourly Price` | `sensor.eprices_tomorrow_current_hourly_price` |
| `Next Hourly Electricity Price Tomorrow` | `sensor.entso_e_prices_next_hourly_electricity_price_tomorrow` | `Tomorrow Next Hourly Price` | `sensor.eprices_tomorrow_next_hourly_price` |
| `Highest Hourly Electricity Price Tomorrow` | `sensor.entso_e_prices_highest_hourly_electricity_price_tomorrow` | `Tomorrow Highest Hourly Price` | `sensor.eprices_tomorrow_highest_hourly_price` |
| `Lowest Hourly Electricity Price Tomorrow` | `sensor.entso_e_prices_lowest_hourly_electricity_price_tomorrow` | `Tomorrow Lowest Hourly Price` | `sensor.eprices_tomorrow_lowest_hourly_price` |
| `Next Day Current Max Hourly Price Percentage` | `sensor.entso_e_prices_next_day_current_max_hourly_price_percentage` | `Tomorrow Current Max Hourly Price Percentage` | `sensor.eprices_tomorrow_current_max_hourly_price_percentage` |
| `Next Day Price Update Attempts` | `sensor.entso_e_prices_next_day_price_update_attempts` | `Tomorrow API Fetch Attempts` *(moved to text_sensor)* | `sensor.eprices_tomorrow_api_fetch_attempts` |
| `Tomorrow Entry Count` | `sensor.entso_e_prices_tomorrow_entry_count` | `Tomorrow Entry Count` *(moved to text_sensor)* | `sensor.eprices_tomorrow_entry_count` |

---

#### Text sensors

| entso-e-prices v4.3.1 name | v4.3.1 entity ID | EPrices v1.2 name | EPrices v1.2 entity ID |
|---|---|---|---|
| `ENTSO-E Hourly Prices EUR⁄kWh JSON` | `sensor.entso_e_prices_entso_e_hourly_prices_eur_kwh_json` | `Today JSON Hourly Prices EUR⁄kWh` | `sensor.eprices_today_json_hourly_prices_eur_kwh` |
| `ENTSO-E 15-Min Prices EUR⁄kWh JSON (P1 00:00-07:45)` | `sensor.entso_e_prices_entso_e_15_min_prices_eur_kwh_json_p1_00_00_07_45` | `Today JSON 15-Min Prices EUR⁄kWh (P1 00:00-07:45)` | `sensor.eprices_today_json_15_min_prices_eur_kwh_p1_00_00_07_45` |
| `ENTSO-E 15-Min Prices EUR⁄kWh JSON (P2 08:00-15:45)` | `sensor.entso_e_prices_entso_e_15_min_prices_eur_kwh_json_p2_08_00_15_45` | `Today JSON 15-Min Prices EUR⁄kWh (P2 08:00-15:45)` | `sensor.eprices_today_json_15_min_prices_eur_kwh_p2_08_00_15_45` |
| `ENTSO-E 15-Min Prices EUR⁄kWh JSON (P3 16:00-23:45)` | `sensor.entso_e_prices_entso_e_15_min_prices_eur_kwh_json_p3_16_00_23_45` | `Today JSON 15-Min Prices EUR⁄kWh (P3 16:00-23:45)` | `sensor.eprices_today_json_15_min_prices_eur_kwh_p3_16_00_23_45` |
| `Time Of Highest Energy Price Today` | `sensor.entso_e_prices_time_of_highest_energy_price_today` | `Today Highest Price Time` | `sensor.eprices_today_highest_price_time` |
| `Time Of Lowest Energy Price Today` | `sensor.entso_e_prices_time_of_lowest_energy_price_today` | `Today Lowest Price Time` | `sensor.eprices_today_lowest_price_time` |
| `Time Of Highest Hourly Energy Price Today` | `sensor.entso_e_prices_time_of_highest_hourly_energy_price_today` | `Today Highest Hourly Price Time` | `sensor.eprices_today_highest_hourly_price_time` |
| `Time Of Lowest Hourly Energy Price Today` | `sensor.entso_e_prices_time_of_lowest_hourly_energy_price_today` | `Today Lowest Hourly Price Time` | `sensor.eprices_today_lowest_hourly_price_time` |
| `Price Update Status` | `sensor.entso_e_prices_price_update_status` | `Today Price Update Status` | `sensor.eprices_today_price_update_status` |
| `Last Price Update Time` | `sensor.entso_e_prices_last_price_update_time` | `Today Data Loaded Time` | `sensor.eprices_today_data_loaded_time` |
| `Price Update Status Message` | `sensor.entso_e_prices_price_update_status_message` | `Today Price Update Status Message` | `sensor.eprices_today_price_update_status_message` |
| `Current Price Status` | `sensor.entso_e_prices_current_price_status` | `Today Current Price Status` | `sensor.eprices_today_current_price_status` |
| `ENTSO-E Next Day Hourly Prices EUR⁄kWh JSON` | `sensor.entso_e_prices_entso_e_next_day_hourly_prices_eur_kwh_json` | `Tomorrow JSON Hourly Prices EUR⁄kWh` | `sensor.eprices_tomorrow_json_hourly_prices_eur_kwh` |
| `ENTSO-E Next Day 15-Min Prices EUR⁄kWh JSON (P1 00:00-07:45)` | `sensor.entso_e_prices_entso_e_next_day_15_min_prices_eur_kwh_json_p1_00_00_07_45` | `Tomorrow JSON 15-Min Prices EUR⁄kWh (P1 00:00-07:45)` | `sensor.eprices_tomorrow_json_15_min_prices_eur_kwh_p1_00_00_07_45` |
| `ENTSO-E Next Day 15-Min Prices EUR⁄kWh JSON (P2 08:00-15:45)` | `sensor.entso_e_prices_entso_e_next_day_15_min_prices_eur_kwh_json_p2_08_00_15_45` | `Tomorrow JSON 15-Min Prices EUR⁄kWh (P2 08:00-15:45)` | `sensor.eprices_tomorrow_json_15_min_prices_eur_kwh_p2_08_00_15_45` |
| `ENTSO-E Next Day 15-Min Prices EUR⁄kWh JSON (P3 16:00-23:45)` | `sensor.entso_e_prices_entso_e_next_day_15_min_prices_eur_kwh_json_p3_16_00_23_45` | `Tomorrow JSON 15-Min Prices EUR⁄kWh (P3 16:00-23:45)` | `sensor.eprices_tomorrow_json_15_min_prices_eur_kwh_p3_16_00_23_45` |
| `Time Of Highest Energy Price Tomorrow` | `sensor.entso_e_prices_time_of_highest_energy_price_tomorrow` | `Tomorrow Highest Price Time` | `sensor.eprices_tomorrow_highest_price_time` |
| `Time Of Lowest Energy Price Tomorrow` | `sensor.entso_e_prices_time_of_lowest_energy_price_tomorrow` | `Tomorrow Lowest Price Time` | `sensor.eprices_tomorrow_lowest_price_time` |
| `Time Of Highest Hourly Energy Price Tomorrow` | `sensor.entso_e_prices_time_of_highest_hourly_energy_price_tomorrow` | `Tomorrow Highest Hourly Price Time` | `sensor.eprices_tomorrow_highest_hourly_price_time` |
| `Time Of Lowest Hourly Energy Price Tomorrow` | `sensor.entso_e_prices_time_of_lowest_hourly_energy_price_tomorrow` | `Tomorrow Lowest Hourly Price Time` | `sensor.eprices_tomorrow_lowest_hourly_price_time` |
| `Next Day Price Update Status` | `sensor.entso_e_prices_next_day_price_update_status` | `Tomorrow Price Update Status` | `sensor.eprices_tomorrow_price_update_status` |
| `Next Day Last Price Update Time` | `sensor.entso_e_prices_next_day_last_price_update_time` | `Tomorrow Data Loaded Time` | `sensor.eprices_tomorrow_data_loaded_time` |
| `Next Day Price Update Status Message` | `sensor.entso_e_prices_next_day_price_update_status_message` | `Tomorrow Price Update Status Message` | `sensor.eprices_tomorrow_price_update_status_message` |
| `Next Day Current Price Status` | `sensor.entso_e_prices_next_day_current_price_status` | `Tomorrow Current Price Status` | `sensor.eprices_tomorrow_current_price_status` |
| `ENTSO-E Last Reboot` | `sensor.entso_e_prices_entso_e_last_reboot` | `Last Reboot` | `sensor.eprices_last_reboot` |
| `Entso-E Today NVS Status` | `sensor.entso_e_prices_entso_e_today_nvs_status` | `Today NVS Status` | `sensor.eprices_today_nvs_status` |
| `Entso-E Tomorrow NVS Status` | `sensor.entso_e_prices_entso_e_tomorrow_nvs_status` | `Tomorrow NVS Status` | `sensor.eprices_tomorrow_nvs_status` |
| `ENTSO-E Last Update Source` | `sensor.entso_e_prices_entso_e_last_update_source` | `Last Update Source` | `sensor.eprices_last_update_source` |
| `Today Data Date` | `sensor.entso_e_prices_today_data_date` | `Today Data Date` *(unchanged)* | `sensor.eprices_today_data_date` |
| `Tomorrow Data Date` | `sensor.entso_e_prices_tomorrow_data_date` | `Tomorrow Data Date` *(unchanged)* | `sensor.eprices_tomorrow_data_date` |

---

#### Buttons

| entso-e-prices v4.3.1 name | v4.3.1 entity ID | EPrices v1.2 name | EPrices v1.2 entity ID |
|---|---|---|---|
| `Entso-E Force Update` | `button.entso_e_prices_entso_e_force_update` | `Force Today's Update` | `button.eprices_force_today_s_update` |
| `Entso-E Force Next Day Update` | `button.entso_e_prices_entso_e_force_next_day_update` | `Force Tomorrow's Update` | `button.eprices_force_tomorrow_s_update` |
| `Entso-E Reboot Device` | `button.entso_e_prices_entso_e_reboot_device` | `Reboot Device` | `button.eprices_reboot_device` |

---

### New sensors in EPrices v1.0 (no equivalent in v4.3.1)

| EPrices v1.2 name | EPrices v1.2 entity ID | Purpose |
|---|---|---|
| `Today Last API Fetch Time` | `sensor.eprices_today_last_api_fetch_time` | Timestamp of last successful HTTP fetch for today |
| `Tomorrow Last API Fetch Time` | `sensor.eprices_tomorrow_last_api_fetch_time` | Timestamp of last successful HTTP fetch for tomorrow |
| `WiFi Signal` | `sensor.eprices_wifi_signal` | RSSI in dBm |
| `Uptime` | `sensor.eprices_uptime` | Human-readable uptime string |

---

### Sensor output changes

| Sensor | v4.3.1 output | EPrices v1.2 output |
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
| `Tomorrow Price Update Status Message` | *(various)* | `"No data yet"` on boot / after reboot (v1.2) |

---

### HA entity ID changes

All entity IDs changed due to the node name change from `entso-e-prices` to `eprices`.

**Step 1 — bulk find and replace in all your automations and dashboards:**
```
sensor.entso_e_prices_  →  sensor.eprices_
button.entso_e_prices_  →  button.eprices_
```

**Step 2 — apply the individual sensor name slug renames** from the tables above
for sensors whose names changed (not just the prefix).

> **Tip:** After the bulk replace in Step 1, search for any remaining
> `entso_e_prices_` strings to catch any that were missed.

### NVS data

The NVS namespace changed from `entsoe2` to `eprices`. On first boot after
flashing EPrices the device will not find any stored data and will
trigger a fresh HTTP fetch automatically. This is expected and safe.
No manual NVS erase is required.

---

## Migration checklist

- [ ] Flash `eprices.yaml` to the device
- [ ] Update `secrets.yaml` — rename all `entsoe_` keys to `eprices_`; add `eprices_prov_fee`, `eprices_vat_rate`, and `eprices_neg_prov_fee`
- [ ] In all HA automations: bulk replace `sensor.entso_e_prices_` → `sensor.eprices_` and `button.entso_e_prices_` → `button.eprices_`
- [ ] Apply individual sensor slug renames from the tables above for sensors whose names changed
- [ ] Remove any external HA automations that handled midnight bridge, boot recovery, or fetch scheduling — these are now all on-device
- [ ] Verify the device fetches fresh data on first boot (check `Today NVS Status` and `Today Price Update Status Message`)
- [ ] Update any HA dashboard cards referencing old entity IDs
- [ ] Verify the JSON sensor entity IDs in dashboards — the `⁄` character slug may differ from expected; check against actual HA entity registry if needed
