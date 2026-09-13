#!/usr/bin/env python3
"""
POST Wi-Fi / MQTT config to a glasslink SoftAP portal (Step 23 / W26).

Join AP `glasslink-setup`, then:

  ./tools/wd_provision.py \\
    --ssid MyWifi --pass secret \\
    --mqtt mqtt://192.168.0.216:1883 --device cyd1

Default portal: http://192.168.4.1/config
"""

from __future__ import annotations

import argparse
import sys
import urllib.error
import urllib.parse
import urllib.request


def main() -> int:
    p = argparse.ArgumentParser(description="POST config to glasslink SoftAP portal")
    p.add_argument("--url", default="http://192.168.4.1/config")
    p.add_argument("--ssid", required=True, help="Wi-Fi SSID")
    p.add_argument("--pass", dest="password", default="", help="Wi-Fi password")
    p.add_argument("--mqtt", required=True, help="mqtt://host:1883")
    p.add_argument("--device", required=True, help="device id e.g. cyd1")
    args = p.parse_args()

    if not args.mqtt.startswith("mqtt://"):
        print("mqtt URI must start with mqtt://", file=sys.stderr)
        return 1

    body = urllib.parse.urlencode(
        {
            "wifi_ssid": args.ssid,
            "wifi_pass": args.password,
            "mqtt_uri": args.mqtt,
            "device_id": args.device,
        }
    ).encode()
    req = urllib.request.Request(
        args.url, data=body, method="POST",
        headers={"Content-Type": "application/x-www-form-urlencoded"},
    )
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            print(resp.read().decode(errors="replace").strip())
            print(f"HTTP {resp.status} — device should reboot into STA")
            return 0
    except urllib.error.URLError as e:
        print(f"POST failed: {e}", file=sys.stderr)
        print("Join Wi-Fi AP 'glasslink-setup' first.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
