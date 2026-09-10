<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# FMO Live Monitor for AI Passport

This firmware turns a FoloToy AI Passport into a small always-on status display
for a local FMO (NFM Over Internet) device. It shows the selected FMO channel,
the callsign currently speaking, the last-heard callsign while idle, grid/host
metadata when available, connection state, and battery level.

## How it works

The monitor uses the local web interface exposed by an FMO device:

- `ws://<host>:<port>/events` supplies `qso/callsign` events with `callsign`,
  `isSpeaking`, `isHost`, and optional `grid` fields.
- `ws://<host>:<port>/ws` receives a periodic
  `station/getCurrent` request so the display can show the selected channel.

Both sockets reconnect automatically. No FMO device certificate or private key
is copied to AI Passport because the firmware observes the local FMO interface;
it does not act as a virtual FMO radio or connect directly to an FMO MQTT server.

## Configure

On first boot, a credential-free build starts a protected hotspot. Join the
`FMO-Setup-XXXX` network using the random password shown on the Passport screen.
Keep the phone connected even if it reports no Internet, then open
`http://192.168.4.1` manually. Choose a nearby 2.4 GHz Wi-Fi network or type a
hidden SSID, enter its password, and submit. The list is scanned once per setup
session. A failed connection can be retried without rebooting.

The firmware verifies Wi-Fi/DHCP for up to 25 seconds before saving credentials
in its own `fmo_wifi` NVS namespace. It does not deliberately delete the previous
credentials on failure. On success, the hotspot and HTTP server shut down and
the monitor connects to `fmo.local:80` using mDNS resolution. Keep both devices on
a LAN that allows multicast and communication between clients.

Long-press **OK** during normal operation to reboot into setup; this also works
when the saved router is unavailable. Existing credentials remain until a new
connection succeeds. During setup, long-press OK is ignored. Power-cycle to retry
a setup startup failure. There is no automatic captive-portal popup or assumption
of shared credentials with other Passport firmware. The setup page is reachable
only through the setup AP, uses a per-session token, and never returns the saved
Wi-Fi password. Credentials are stored in ordinary NVS, not encrypted storage.

Advanced users can still set build-time Wi-Fi fallback, FMO host/port and
brightness via `idf.py menuconfig` → **FMO Live Monitor**. Saved settings take
priority over build-time Wi-Fi values. Do not distribute builds with credentials.

## Build and install

Activate ESP-IDF 5.5.3, then run:

```bash
./tools/validate.sh --static
./tools/validate.sh --setup
```

The installable merged image is
`build/FoloToy-AI-Passport-full.bin`. Keep the protected `cardid` and Recovery
partitions intact. Prefer the AI Passport mini-program installer on a provisioned
device; never erase the full flash of a provisioned device.

## Controls

- **UP**: increase backlight brightness by 10%.
- **DOWN**: decrease backlight brightness by 10%.
- **OK**: refresh the current FMO channel immediately.
- **Long OK**: restart into Wi-Fi setup without deleting existing credentials.

## FMO compatibility

The implementation follows the interface used by current community FMO web
clients. FMO firmware variants may change local event field names or paths. If
the screen connects but never shows a callsign, capture sanitized `/events`
JSON from the FMO browser interface and update `parse_fmo_message()` in
`main/fmo_network.c`. Never publish Wi-Fi passwords, FMO secrets, certificates,
private keys, or unsanitized logs.

## Validation still required on hardware

- Confirm the Passport connects to the intended 2.4 GHz network.
- Confirm the current channel matches the FMO screen after boot and after an
  FMO-side channel change.
- Key and release a radio and verify `ON AIR`, callsign, host/grid metadata, and
  `LAST HEARD` transitions.
- Leave both devices running through a Wi-Fi interruption and confirm automatic
  reconnection.
- Check USB logs, minimum free heap, button response, display clipping, and
  battery display on the physical board.


## Reliability and resource use

- State is reduced under a mutex and sent as a one-slot snapshot. The UI always
  receives the latest result, even when it cannot consume every PTT transition.
- Both abnormal disconnects and clean CLOSE handshakes reconnect. Failed client
  creation is retried while Wi-Fi is available.
- Channel queries run every second and after a new talker. Until confirmation,
  the UI shows synchronization instead of attributing speech to an old channel.
  Changing channel clears prior callsigns. Confirmation expires after five seconds.
- The local event protocol does not carry a channel UID. Across the two sockets,
  channel attribution is best effort: a switch clears ambiguous speech and waits
  for a later event; polling cannot provide an atomic channel/speaker snapshot.
- The bounded text assembler handles transport chunks, continuation frames and
  interleaved control frames. Oversized/invalid frames invalidate live speech.
- Labels update only when content changes. Unused demo sources, BLE, and LVGL
  examples are excluded. Recovery BLE is in the permanent factory image.
- Every minute, logs report internal heap minimum/largest block and coordinator
  and WebSocket stack headroom. Battery stack headroom is logged at DEBUG level.
  Stack allocations remain conservative until measured on a device.

## Validation versus installation

`./tools/validate.sh` runs host checks and a **network-enabled validation build**
using public dummy settings in `tests/sdkconfig.network`. Its image goes to
`build/validation/FoloToy-AI-Passport-full.bin`; it is not an installation image.
The validation build cannot overwrite your installation image.
For a shareable, credential-free setup image, run `./tools/validate.sh --setup`.
It uses tracked defaults only, ignores private `sdkconfig`, and writes
`build/FoloToy-AI-Passport-full.bin` for phone provisioning after installation.
Use `idf.py menuconfig`, then `./tools/validate.sh --configured` for your device.
That command copies your local configuration into a private temporary file,
validates it, and emits `build/FoloToy-AI-Passport-full.bin`. The image contains
your Wi-Fi settings: do not publish it. CI artifacts contain dummy settings only.

The project derives from [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport).
Interface reference: [FMO web client API](https://github.com/niufox/fmo-mobile-controller/blob/main/API_DOCUMENTATION_v2.md).
