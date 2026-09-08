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

Use ESP-IDF 5.5.3 and configure the private values in the ignored `sdkconfig`
file:

```bash
idf.py menuconfig
```

Open **FMO Live Monitor** and set:

- the 2.4 GHz Wi-Fi SSID and password;
- the FMO host name or IPv4 address, without `ws://`, port, or path;
- the FMO web-interface port, normally `80`;
- initial display brightness.

An IPv4 address is recommended if the local network does not resolve
`fmo.local`. The tracked defaults intentionally contain no credentials. With no
SSID or FMO host configured, the screen displays `SET WIFI + FMO HOST` and does
not start the radio stack.

## Build and install

Activate ESP-IDF 5.5.3, then run:

```bash
./tools/validate.sh --static
./tools/validate.sh --configured
```

The installable merged image is
`build/FoloToy-AI-Passport-full.bin`. Keep the protected `cardid` and Recovery
partitions intact. Prefer the AI Passport mini-program installer on a provisioned
device; never erase the full flash of a provisioned device.

## Controls

- **UP**: increase backlight brightness by 10%.
- **DOWN**: decrease backlight brightness by 10%.
- **OK**: refresh the current FMO channel immediately.

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
The validation build cannot overwrite your configured installation image.
Use `idf.py menuconfig`, then `./tools/validate.sh --configured` for your device.
That command copies your local configuration into a private temporary file,
validates it, and emits `build/FoloToy-AI-Passport-full.bin`. The image contains
your Wi-Fi settings: do not publish it. CI artifacts contain dummy settings only.

The project derives from [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport).
Interface reference: [FMO web client API](https://github.com/niufox/fmo-mobile-controller/blob/main/API_DOCUMENTATION_v2.md).
