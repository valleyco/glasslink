# SoftAP + HTTP field provision (W26 / Step 23)

Configure Wi‑Fi SSID/password, MQTT URI, and device id **without** USB `flash-nvs`.

## When it runs

Firmware enters setup if:

1. NVS / Kconfig SSID is empty or `CHANGE_ME`, or
2. STA connect fails within ~25 s

Glass shows a **red SETUP** banner with AP name and URL.

## Phone / laptop

1. Join open Wi‑Fi **`glasslink-setup`**
2. Open **http://192.168.4.1/** (or `/config`)
3. Submit SSID, password, `mqtt://…`, device id
4. Device saves NVS and reboots into STA + MQTT

CLI (after joining the AP):

```bash
./tools/wd_provision.py \
  --ssid MyWifi --pass 'secret' \
  --mqtt mqtt://192.168.0.216:1883 --device cyd1
```

## Factory / lab

`make flash-nvs` from `tools/nvs.csv` still works and skips SoftAP when STA connects.

## Security

Open SoftAP, plaintext HTTP — LAN / short-lived setup only (W10). No TLS.
STA-mode config API while online = follow-on.
