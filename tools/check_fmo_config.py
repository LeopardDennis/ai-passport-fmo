"""Validate a private sdkconfig without printing credentials."""
import json
import sys
from pathlib import Path


def validate(path):
    values = {}
    for line in Path(path).read_text().splitlines():
        if line.startswith("CONFIG_FMO_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = json.loads(value)
    ssid = values.get("CONFIG_FMO_WIFI_SSID", "")
    host = values.get("CONFIG_FMO_HOST", "")
    password = values.get("CONFIG_FMO_WIFI_PASSWORD", "")
    if not isinstance(ssid, str) or not 1 <= len(ssid.encode()) <= 32:
        raise ValueError("SSID must contain 1 to 32 bytes")
    if not isinstance(host, str) or not host or len(host) > 120 or any(
        c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-" for c in host
    ):
        raise ValueError("FMO host must be a hostname or IPv4 address, without port/path")
    if not isinstance(password, str) or (password and not 8 <= len(password.encode()) <= 63):
        raise ValueError("Wi-Fi password must be empty or contain 8 to 63 bytes")


if __name__ == "__main__":
    try:
        validate(sys.argv[1])
    except (OSError, ValueError, IndexError):
        sys.exit("Invalid/missing FMO configuration; run idf.py menuconfig first.")
