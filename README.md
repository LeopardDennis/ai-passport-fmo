<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# FMO Live Monitor for AI Passport

This firmware turns a FoloToy AI Passport into a small connected status display
for a local FMO (NFM Over Internet) device. It shows the selected FMO channel,
the callsign currently speaking, the last-heard callsign while idle, the three most recent
contact records, connection state, and battery level.

## How it works

The monitor uses the local web interface exposed by an FMO device:

- `ws://<host>:<port>/events` supplies `qso/callsign` events with `callsign`,
  `isSpeaking`, `isHost`, and optional `grid` / `crossServer` fields.
- `ws://<host>:<port>/ws` receives a periodic
  `station/getCurrent` request so the display can show the selected channel.
- `qso/history` on `/events` supplies recent callsigns and UTC timestamps;
  the monitor displays the newest three records without querying saved-log details.

The optional `ws://<host>:<port>/audio` connection receives conversation audio.
All enabled sockets reconnect automatically. No FMO device certificate or private key
is copied to AI Passport because the firmware observes the local FMO interface;
it does not act as a virtual FMO radio or connect directly to an FMO MQTT server.

## Configure

The portrait UI uses black, orange and white based on the supplied FMO reference
images (not an official color specification). The large callsign belongs to the
active speaker: bold orange for ordinary speech, red when `crossServer` is true
(or 1), and bold white for the last-heard callsign while
idle. The grid follows the large callsign in smaller gray text and remains after
release until the next speaker replaces it; missing grids are hidden. Elapsed-time counters and station/host prefixes are hidden.
The thin yellow bar shows the RMS strength of conversation PCM sent to playback,
with a quick rise and smooth fall. It uses the source audio level independently
of speaker volume and clears when muted, disconnected, or viewing setup/menus.
Quiet input is gated; missing audio leaves it empty. It is not an RF measurement.
Unknown metadata is not simulated. Callsigns use Montserrat Bold, setup passwords use
the regular built-in weight, and Noto Sans CJK covers Chinese channel names, not a claim to match the original FMO typeface.
Status and controls are in Chinese; callsigns, network names and addresses stay
unchanged. The iOS-inspired battery capsule contains a numeric percentage, turns
red at 20% or below, and shows an outlined unknown state when SOC is unavailable.
It does not infer charging. Channel and callsign text is centered using visible
glyph bounds, including smaller fonts for long callsigns and setup passwords.

The yellow panel is titled `QSO` and shows the three most recent contacts reported
by FMO, newest first. Each record uses two lines: a white bold callsign followed
by its Beijing date/time (`YYYY-MM-DD HH:MM:SS`) in regular gray text.
Repeated callsigns at different times
remain separate contacts. Missing slots/times show dashes. History updates only
when FMO pushes `qso/history`; it does not overwrite the live callsign, speech
state or channel-query ownership. Losing Wi-Fi/event connectivity clears history;
control-query delays do not clear it. The protocol does not supply per-record
frequency, mode, remarks or grid; saved QSO details and local `config/getUserPhy*`
settings are not queried for these rows. Status, bold callsign and the yellow
PCM level bar remain above the panel. Status reads in contact during a transmission
and last contact after release. The speaker grid is shown beside the callsign
in a separate small gray label, retained after release until the next speaker;
long callsigns scroll within
the remaining width without overlapping it.

The header shows the current time between FMO and the battery as `HH:MM`, in
Beijing time (UTC+8). Wi-Fi starts a non-blocking SNTP service using
`pool.ntp.org`; before the system clock is valid it shows `--:--`. Reconnection
restarts synchronization without allocating another service. An already synced
clock continues during a network interruption, and wake-up redraws the current
minute. Time synchronization requires access to the NTP server. Separate transmit/receive frequencies are not displayed.

Conversation audio connects automatically once Wi-Fi is ready, at 50% volume
by default. Its connection and activity watchdog are independent of the channel
and event sockets: recovering metadata does not mute or discard healthy audio. The `/audio` interface supplies 8 kHz,
16-bit signed little-endian mono PCM; the existing ES8311/I2S driver plays it
in a separate worker. A fixed 24 KiB queue holds up to 1.536 seconds of PCM and handles split
WebSocket messages, an initial 100 ms buffer target and underrun silence. Before
accepting a new message, the socket worker reserves space for the whole message
and yields to playback for up to two seconds when full. This applies TCP
backpressure instead of immediately discarding burst audio, without increasing
RAM. Only a stalled consumer beyond that wait falls back to dropping oldest samples. TCP chunks and continuation frames are decoded in that same ring
but become playable only when the complete WebSocket message arrives; an
unfinished message cannot play its first words before its delayed remainder.
While audio is enabled, the worker disables Wi-Fi modem sleep to reduce receive
jitter, and restores the previous power mode on mute, offline or codec failure.
Keeping Wi-Fi awake increases power consumption during audio playback.
Muting or losing connectivity clears queued audio and closes only
the audio socket; playback resumes with fresh data and a silent DMA prime.
The audio socket has a separate 10-second transport wait. It keeps sending
heartbeats but reconnects for inactivity only after 90 seconds without valid
PCM or PING/PONG activity, so a receiving stream is not cut off solely for a
missing PONG. TCP errors and peer resets still reconnect. Audio
logs report connection changes and aggregate received bytes, discarded samples
and invalid messages, committed/pending samples, output/source samples and
maximum codec-write duration and buffer-wait time every 30 seconds, without conversation content.
Audio continues while the LCD sleeps. No microphone capture, recording or
transmit/PTT commands are added. The footer uses one centered row with the audio percentage and the long-OK setup hint. It displays the selected volume while audio is enabled and 0% while muted; clicking OK toggles sound and holding OK opens the setup/network menu.
With no active speech, the middle shows the last-heard callsign in bold white;
before the first contact it shows `--` and a waiting-for-speech hint.
Runtime sound settings reset on reboot; `CONFIG_FMO_VOLUME` sets initial volume.
Audio/codec failures leave the monitor running. Physical playback, concurrent
RAM/stack headroom and end-to-end latency still require device validation.
The interface is also described in the [FMO web client API documentation](https://github.com/niufox/fmo-mobile-controller/blob/main/API_DOCUMENTATION_v2.md).

Native UI checks and actual LVGL framebuffer previews can be built after the
firmware dependencies have been fetched:

```bash
cmake -S tests/ui_preview -B /tmp/fmo-ui-preview
cmake --build /tmp/fmo-ui-preview
ctest --test-dir /tmp/fmo-ui-preview --output-on-failure
/tmp/fmo-ui-preview/fmo_ui_preview onair /tmp/fmo-onair.ppm
```

Other preview states are `idle`, `muted`, `lastheard`, `offline`, `setup` (QR),
`setup_info` (credentials), `network`, `long`, `rare`, `error`, and
`meter_low` / `meter_mid` / `meter_high` (audio strength).

Up to five verified Wi-Fi networks are saved. The previous single-network format
is imported automatically. Updating an existing SSID replaces its password only
after a successful connection; a sixth distinct network is rejected until one
is deleted. The setup page lists saved SSIDs (never passwords), provides explicit
deletion with confirmation, and has a finish button to exit without adding a
network. Deleting all entries keeps setup available and does not restore legacy
credentials. Open Wi-Fi setup from the network menu to add another network after provisioning.

Boot tries the last successful network first. While disconnected, each connection
attempt gets up to 25 seconds, followed by other saved networks ordered by scanned
signal strength; unseen/hidden entries are also tried. After exhausting the list,
the firmware waits 30 seconds before another round. A healthy connection is never
switched merely for stronger signal or because FMO is unavailable. Wi-Fi loss
closes the old FMO sockets; reconnection creates fresh clients for the new LAN.
The new LAN must also provide access to FMO. Only a change of preferred network
is written to NVS, not every reconnect. WPA2-or-newer personal networks and open
networks are supported, not enterprise authentication.

On first boot, a credential-free build starts a protected hotspot. Join the
`FMO-Setup-XXXX` network using the random password shown on the Passport screen.
Keep the phone connected even if it reports no Internet, then open
`http://192.168.9.1` manually. Choose a nearby 2.4 GHz Wi-Fi network or type a
hidden SSID, enter its password, and submit. The list is scanned once per setup
session. A failed connection can be retried without rebooting.
When retrying a different network, the setup hotspot may briefly disconnect;
rejoin it with the displayed password if the page stops responding.

The firmware verifies Wi-Fi/DHCP for up to 25 seconds before saving credentials
in its own `fmo_wifi` NVS namespace. It does not deliberately delete the previous
credentials on failure. On success, the hotspot and HTTP server shut down and
the monitor connects to the saved FMO target (default `fmo.local:80`, using mDNS resolution). Keep both devices on
a LAN that allows multicast and communication between clients.

The setup page's **FMO address** form saves a hostname or IPv4 address and port,
with `fmo.local:80` as the default. Saved values override build-time settings and
take effect after setup finishes, without rebuilding. **Check port** uses a saved
Wi-Fi profile to check DNS resolution and TCP reachability. Save a Wi-Fi profile
first, then re-enter setup to check it. A reachable port does not prove WebSocket
path or event compatibility; the monitor screen reports the final FMO status.
A check neither saves new Wi-Fi credentials nor changes the saved FMO address.

Network/setup startup failures retain the worker and retry every five seconds;
the network menu's retry interrupts the wait. Partial initialization unregisters
event handlers and releases Wi-Fi resources, and recovery clears the error.
Installation cleanup failures continue retrying without enabling networking;
identity and Recovery remain protected.

Long-press **OK** to open the network menu. Use UP/DOWN to select **Wi-Fi setup**,
**Retry Wi-Fi**, or **Return to monitor**, then click OK. Long OK returns. Setup
restarts into the QR screen; it remains available when the old router is offline.
Retry immediately tries saved networks, without disconnecting a healthy link.
During setup, click OK to switch between the connection QR and the hotspot name
and password. Scan with your phone camera to join, or connect manually using the
displayed credentials. Long OK cancels setup, closes the hotspot, and restores
the current saved network list without persisting an unverified password or
restoring networks deleted on the web page. Startup failures retry automatically and remain accessible from the network menu. There is no automatic captive-portal popup or assumption
of shared credentials with other Passport firmware. The setup page is reachable
only through the setup AP, uses a per-session token, and never returns the saved
Wi-Fi password. Credentials are stored in ordinary NVS, not encrypted storage.

Advanced users can still set build-time Wi-Fi fallback, FMO host/port and
brightness and initial audio volume via `idf.py menuconfig` → **FMO Live Monitor**. Saved settings take
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

## Fresh-install data reset

Installing the complete FMO package clears all application data on its first
boot: saved Wi-Fi networks/passwords, previous FMO settings, and PDKPASS calendar,
standings, results, reminders and switches. This also applies when reinstalling
the same version. Ordinary power cycles and long-OK Wi-Fi setup keep the current
installation's data. Device identity and permanent Recovery are preserved.

The package includes a reset request in `fmo_install`; cleanup completes before
NVS/Wi-Fi initialization and only then records the active image fingerprint.
Interrupted cleanup is retried on boot; errors prevent networking with old data.
`pdk_cache` remains declared solely to erase the legacy cache safely. Use the
complete package: flashing only the application omits the fresh-install request
and cannot reliably identify a reinstall of the same binary.

## Controls

- **UP**: increase conversation volume by 10%, up to 100%.
- **DOWN**: decrease conversation volume by 10%, down to 0%.
- **OK**: toggle audio on/off while preserving the selected volume.
- **Long UP**: refresh the current FMO channel immediately (the five-second boot Recovery hook is unchanged).
- **Long OK**: open the network menu; UP/DOWN selects, click OK executes, long OK returns.
- **Setup click OK**: switch QR/credentials; long OK cancels to the network menu.

## Idle display and wake-up

With no speech or button activity, the screen dims to at most 20% after 30 seconds
and switches off after 90 seconds. The LCD enters panel sleep and its refresh
pauses; Wi-Fi, FMO event/control connections, enabled audio playback and button scanning remain active. A valid
speech-start event or any button wakes it. The first button gesture only wakes
the screen, including a long press; use the next gesture for the usual controls.
Continuous speech, the network menu, and Wi-Fi setup keep the screen awake. Wake-up redraws the
latest state before restoring brightness and refreshes the battery reading.
Battery polling pauses while the screen is off. This is display power saving,
not MCU deep sleep; battery-life improvements require measurement on hardware.

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
- Key and release a radio and verify the speaking indicator,
  bold callsign and last-heard transitions, with the gray grid retained after release and replaced by the next speaker.
  With a stable connection and confirmed channel, starts, talker changes and
  releases should not show the synchronization screen.
- Leave both devices running through a Wi-Fi interruption and confirm automatic
  reconnection.
- Confirm 30-second dimming, 90-second screen-off, button and speech wake-up,
  and uninterrupted FMO connections while the screen is off.
- Check USB logs, minimum free heap, largest allocation, task stack headroom,
  button response, display clipping and battery display on the physical board.
- Measure bright/dim/dark current before adjusting polling or stack allocations.
- Verify saved FMO addresses, LAN access, port checks and menu retry after startup failure.


## Reliability and resource use

- Channel queries that time out after ten seconds or are partially sent recreate the control client before another query. The protocol has no request IDs, so old replies cannot inherit a new query's speaker revision.
- Split the font losslessly into two fallback fonts with compact glyph descriptors; preserve all existing characters and rendered pixels.

- State is reduced under a mutex and sent as a one-slot snapshot. The UI always
  receives the latest result, even when it cannot consume every PTT transition.
- Chinese channel names are retained as UTF-8. Bounded copies truncate only at
  character boundaries; invalid text falls back to the channel UID.
- Explicit `isSpeaking: false/0` ends speech without invalidating the confirmed
  channel or last-heard display. Empty, null or omitted callsigns release the current
  talker; named releases must match. Release grid/host metadata is unused, and
  duplicates do not reset the last-heard time. Starts still require a valid callsign
  and validated optional metadata.
- Invalid JSON, invalid callsigns/start metadata, and raw or JSON-escaped NUL clear live
  speech and require a fresh channel query. Replies predating a new speaker do
  not erase that speaker, invalidate a confirmed channel, or restore an unknown channel.
- All three WebSocket connections ping every ten seconds. Events/control allow
  thirty seconds for PONG; audio uses a ninety-second valid-activity deadline.
  Control writes and events/control reads allow three seconds; audio reads allow
  ten seconds. Query waits retain the confirmed channel without replacing an
  outstanding request. Logs identify socket failures, invalid live metadata,
  channel expiry and coordinator reconnect reasons.
- Profile rows, speech status and callsign positions stay fixed from initial empty data through active speech and last-heard display.
- Both abnormal disconnects and clean CLOSE handshakes reconnect. Failed client
  creation is retried while Wi-Fi is available.
- Channel queries run every second and after a new talker. With a confirmed
  channel, speech starts immediately on screen while the query runs in the
  background. When channel confirmation is missing, only the connection/channel
  rows show that confirmation is pending; callsigns and grids from a connected
  event stream remain visible. An unconfirmed cached channel name is hidden.
  Audio and its level bar follow their own stream, even if event metadata recovers.
  A reply from
  before the latest talker may renew an already-valid channel with the same UID
  without changing speech. Replies for a different or unknown channel are
  discarded and retried, and cannot restore an expired channel. This prevents
  frequent talker changes from starving same-channel confirmations.
  Changing channel clears prior callsigns. Confirmation expires after fifteen seconds.
- The local event protocol does not carry a channel UID. Across the two sockets,
  channel attribution is best effort: a switch clears ambiguous speech and waits
  for a later event; polling cannot provide an atomic channel/speaker snapshot.
- The bounded text assembler handles transport chunks, continuation frames and
  interleaved control frames. Each text socket has a 2 KiB buffer so normal
  20-entry QSO history lists fit and leave current speech/channel state intact.
  Oversized/invalid frames still invalidate live speech; diagnostics include
  socket and frame sizes without logging payloads.
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

Private builds normalize `LV_FONT_FMT_TXT_LARGE` only in their temporary config
copy, preserving the original. Existing configurations used with `idf.py build`
must disable that option to match the compact font.
