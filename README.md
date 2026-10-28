<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# FMO Live Monitor for AI Passport

This firmware turns a FoloToy AI Passport into a small connected status display
for a local FMO (NFM Over Internet) device. It shows the selected FMO channel,
the callsign currently speaking, the last-heard callsign while idle, the two most recent
contact records, connection state, and battery level.

## How it works

The monitor uses the local web interface exposed by an FMO device:

- `ws://<host>:<port>/events` supplies `qso/callsign` events with `callsign`,
  `isSpeaking`, `isHost`, and optional `grid` / `crossServer` fields.
- `ws://<host>:<port>/ws` receives a periodic
  `station/getCurrent` request so the display can show the selected channel.
- `qso/history` on `/events` supplies recent callsigns and UTC timestamps;
  the monitor displays the newest two records without querying saved-log details.

The optional `ws://<host>:<port>/audio` connection receives conversation audio.
All enabled sockets reconnect automatically. No FMO device certificate or private key
is copied to AI Passport because the firmware observes the local FMO interface;
it does not act as a virtual FMO radio or connect directly to an FMO MQTT server.

## Speaker location

The gray slot beside the callsign always displays the Maidenhead grid code.
A gray line above the yellow audio bar and outside the QSO panel shows the
current or last-heard speaker's short place name, bearing, and distance, in
that order. Common Chinese administrative names are shortened for display,
for example Inner Mongolia without its autonomous-region suffix, or Foshan
without its city suffix. Place names include city plus district/county when
available, with the city first. The cache format is versioned so older
single-region labels are refreshed.
The original geocoder/cache value is retained. The 216 px row reserves room for
the complete bearing and distance. Overflowing place names scroll back and forth
at 30 pixels per second, with a one-second pause at both ends; bearing and distance
stay fixed. Short names remain still. Scrolling pauses while the display sleeps
and stops on menu pages. Unchanged snapshots do not restart it.
Missing or pending place names show `--`. The place belongs to the live/last
speaker, independently of the two history records. The callsign keeps its
fixed 32 px size and scrolls if needed.

The origin is the coordinate configured on the connected FMO, read through
`config/getCordinate` on `/ws` after the channel is confirmed, then at most once
per minute. It is not the Passport's measured position. Numeric latitude and
longitude are required; missing, out-of-range or `(0,0)` default coordinates
leave distance/bearing as `--`. Control or Wi-Fi disconnection invalidates the
origin. Coordinate replies have separate ownership from channel and station
requests; an unanswered read expires after ten seconds and is not retried on
that connection, so unsupported firmware does not force a reconnect.

Distance is a great-circle estimate, and bearing is the initial direction from
the FMO coordinate toward the speaker, clockwise from true north (not magnetic
north or the device's orientation), with an eight-point compass abbreviation.
Speaker WGS84 coordinates take priority; otherwise the grid center is used and
both sources use the same display format. Coarse grids can introduce
large distance/direction errors. Missing speaker positions show placeholders;
coincident points and ambiguous polar/antipodal bearings show `--` for direction.
These calculations need no Internet and retain the last contact after release.
The actual FMO coordinate response and device layout still require hardware
validation. Fixed distance/bearing text is drawn directly to preserve the
24 KiB LVGL budget.

A lifetime background worker converts 4/6/8-character Maidenhead grids to their
cell centers and queries [Photon](https://github.com/komoot/photon). Optional
numeric `latitude` / `longitude` fields in a `qso/callsign` start event take
priority as WGS84 decimal degrees. Invalid/missing pairs fall back to the grid.
The reference FMO client documents grids; coordinate fields from other firmware
variants still need device verification. A grid represents an area, so its
center-derived city can differ near boundaries. The place name itself does not
claim more precision than the source coordinate.

The default HTTPS endpoint is `https://photon.komoot.io/reverse`. Only coordinates
are sent, without callsigns, FMO credentials, or Wi-Fi secrets. TLS certificates
and hostname are verified using the common CA bundle; uncached lookups wait for
a valid system clock and sufficient heap. Internet/DNS/TLS failures leave the
grid intact. Cached places work on a connected local FMO network without Internet.
The public demo permits reasonable use but has no availability guarantee; a
larger deployment should set `CONFIG_FMO_GEOCODER_URL` to its own Photon HTTPS
endpoint. An empty value disables lookup. This endpoint is selected at build time.
Location data © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright);
attribution is also shown on the setup web page.

The worker polls the latest speaker every 200 ms and never blocks UI, event
processing, or audio tasks. Requests start at least three seconds apart, with
only one request in flight; a slow completed request adds no further fixed wait.
Transient failures use a 30-second negative cache per coordinate and a global
5/10/20/40/60-second exponential backoff. HTTP 429/503 waits at least 60 seconds;
a successful lookup resets the backoff. Cache hits bypass these waits. Logs record
query duration and cache hits without recording coordinates or speaker identities.
The HTTP body is capped at 4 KiB, each socket wait at three seconds, and the
body-read loop has an eight-second deadline starting after connection and headers
(a blocking socket operation may finish after the deadline). Rate-limited logs
identify clock/heap waits and query failures without logging callsigns or coordinates. A short transmission can finish before lookup returns;
the result still updates the last contact if its channel, callsign and position
match. Stale results are cached without overwriting a newer speaker.

Up to 32 successful places persist in ordinary application NVS, bounded to
roughly 4 KiB of NVS entries plus namespace/page overhead; the RAM cache is about
2.2 KiB. The worker has a 6 KiB stack, with additional transient HTTP/TLS heap. Default builds use on-demand TLS record buffers,
1 KiB outgoing records and the standard 16 KiB incoming limit. The worker requires
56 KiB free internal heap and a 32 KiB contiguous block before querying; custom
builds without dynamic buffers or with larger outgoing records retain the 64 KiB
free-heap gate. The 4 KiB response buffer is allocated after the handshake peak.
Empty cache slots are filled before evicting entries in bounded rotation, including
a partially populated cache restored after reboot. Successful entries have no time-based expiry. Reboot
retains them; firmware installation cleanup removes them. Only successful new
queries write flash. Storage errors retain the RAM result. Physical TLS/audio
coexistence and lookup latency remain device checks.

## Configure

The portrait UI uses black, orange and white based on the supplied FMO reference
images (not an official color specification). The large callsign belongs to the
active speaker: bold orange for ordinary speech, red when `crossServer` is true
(or 1), and bold white for the last-heard callsign while
idle. The original grid follows the callsign in smaller gray text, while the
short place name shares a line with bearing and distance above the yellow audio bar and outside the QSO panel. Both remain
after release. Missing grids are hidden; missing place names show `--`.
Elapsed-time counters and station/host prefixes are hidden.
The thin yellow bar shows the RMS strength of conversation PCM sent to playback,
with a quick rise and smooth fall. It uses the source audio level independently
of speaker volume and clears when muted, disconnected, or viewing setup/menus.
Quiet input is gated; missing audio leaves it empty. It is not an RF measurement.
Unknown metadata is not simulated. Callsigns use Montserrat Bold, setup passwords use
the regular built-in weight, and Noto Sans CJK covers Chinese channel names, not a claim to match the original FMO typeface.
Status and controls are in Chinese; callsigns, network names and addresses stay
unchanged. The iOS-inspired battery capsule contains a numeric percentage, turns
red at 20% or below, and shows an outlined unknown state when SOC is unavailable.
Unavailable readings are retried once a minute while the screen is awake, including
a failed startup initialization. Healthy readings retain the two-minute interval;
screen-off polling pauses and wake requests an immediate sample. Initialization
retries restore and verify the original-cell profile when needed; invalid runtime
readings do not periodically reset an already-initialized gauge. Recovery after
a depleted battery still requires physical-device validation.
It does not infer charging. Channel and callsign text is centered using visible
glyph bounds, including smaller fonts for long setup passwords.

The yellow panel is titled `QSO` and shows the two most recent contacts reported
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
callsigns retain a fixed 32 px font and a 4 px gap to the grid. Only the grid
font becomes smaller when needed, preserving six-character alphanumeric callsigns
and the complete grid on one line without scrolling or ellipsis. QSO callsigns
also remain static.

The header shows the current time between FMO and the battery as `HH:MM`, in
Beijing time (UTC+8). Wi-Fi starts a non-blocking SNTP service using
`pool.ntp.org`; before the system clock is valid it shows `--:--`. Reconnection
restarts synchronization without allocating another service. An already synced
clock continues during a network interruption, and wake-up redraws the current
minute. Time synchronization requires access to the NTP server. Separate transmit/receive frequencies are not displayed.

Conversation audio connects automatically once Wi-Fi is ready, at 80% volume
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
transmit/PTT commands are added. The connection row shows status on the left and a right-aligned audio percentage on the right, aligned with the channel panel edge. It displays the selected volume while audio is enabled and 0% while muted. The single-line footer uses a 14 px font to describe short confirm for the station list and long confirm for Wi-Fi setup. The network menu and Wi-Fi list show UP/DOWN selection and long-confirm return; the station footer shows long-UP/DOWN paging and long-confirm return. All page footers use the same 14 px font, one centered line and consistent Chinese names for the confirm key; empty Wi-Fi lists and station loading/retry states show applicable actions. Callsigns stay at 32 px without scrolling or adaptive sizing; only the grid font adapts to the available width. Set volume to 0% to silence playback.
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
`setup_info` (credentials), `network`, `wifi_saved` (saved Wi-Fi selection), `wifi_empty`, `wifi_current`, `wifi_offline`, `wifi_long`, `long`, `rare`, `error`, and
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

On first boot, a credential-free build starts a protected hotspot and displays
the three-action network menu. Select **Wi-Fi setup** with UP/DOWN and click OK
to open the connection QR. Join the
`FMO-Setup-XXXX` network using the random password shown on the Passport screen.
Keep the phone connected even if it reports no Internet, then open
`http://192.168.9.1` manually. Choose a nearby 2.4 GHz Wi-Fi network or type a
hidden SSID, enter its password, and submit. The password field does not request
a new password; the form and field request autocomplete off, with correction,
capitalization and spellcheck disabled. System password-manager suggestions may
still appear. The list is scanned once per setup
session. A failed connection can be retried without rebooting.
When retrying a different network, the setup hotspot may briefly disconnect;
rejoin it with the displayed password if the page stops responding.

The firmware verifies Wi-Fi/DHCP for up to 25 seconds before saving credentials
in its own `fmo_wifi` NVS namespace. It does not deliberately delete the previous
credentials on failure. On success, the hotspot and HTTP server shut down and
the monitor connects to the saved FMO target (default `fmo.local:80`, using mDNS resolution). Keep both devices on
a LAN that allows multicast and communication between clients.

Each saved Wi-Fi profile includes its own **FMO address** (hostname or IPv4) and
port, defaulting to `fmo.local:80`. **Connect and save Wi-Fi and FMO address** saves
both together after Wi-Fi/DHCP succeeds. Selecting a saved network on the setup
page restores its address; **Update only the saved network's FMO address** edits
that destination without entering the password again. Rebooting or switching
Wi-Fi restores the associated destination for events, control and audio. When legacy NVS data is present, existing
profiles inherit the former global destination during migration; new networks
use the build-time default. **Check port** connects to the selected saved Wi-Fi
profile to check DNS resolution and TCP reachability. Save a Wi-Fi profile
first, then re-enter setup to check it. A reachable port does not prove WebSocket
path or event compatibility; the monitor screen reports the final FMO status.
The existing fresh-install reset still clears application settings when flashing a new firmware image.
A check neither saves new Wi-Fi credentials nor changes the saved FMO address.

Network/setup startup failures retain the worker and retry every five seconds;
a network-selection request interrupts the wait. Partial initialization unregisters
event handlers and releases Wi-Fi resources, and recovery clears the error.
Installation cleanup failures continue retrying without enabling networking;
identity and Recovery remain protected.

On the main screen, long-press **OK** to open the network menu. Use UP/DOWN to select **Wi-Fi setup**,
**Select Wi-Fi**, or **Return**, then click OK. Long OK returns. Setup
restarts into the network menu; click OK on Wi-Fi setup to open the QR screen.
It remains available when the old router is offline.
Select Wi-Fi opens a list of up to five saved Wi-Fi names. Use UP/DOWN to select a name
and click OK to connect; long OK returns to the network menu. The actual connected
network is marked with a small check on the right, and the selected long name scrolls. An empty list
cannot start a connection. Selecting the currently connected network keeps Wi-Fi
and immediately retries failed FMO connections, leaving healthy FMO links intact.
Selecting another network closes old FMO connections and restarts
station association before trying the chosen saved credentials. A failed attempt
falls back to the normal automatic profile retry after 25 seconds. Network names
are resolved again by the worker, so deleting or reordering profiles on the web
page cannot connect a different entry accidentally. Only a successful connection
changes the preferred network; Wi-Fi passwords never enter the UI state.
During setup, click OK to switch between the connection QR and the hotspot name
and password. Scan with your phone camera to join, or connect manually using the
displayed credentials. Click UP/DOWN or long-press OK to cancel setup and return
to the network menu. Confirming a saved network while the hotspot is active exits setup
before connecting; Return or long OK in the menu also closes
the hotspot. Repeated setup snapshots preserve the selected menu row. Cancellation
closes the hotspot and restores
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
the same version. Ordinary power cycles and Wi-Fi setup from the network menu keep the current
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
- **Main-screen OK**: open the station category menu (favorites, active, all).
- **Main-screen long OK**: open the network menu for Wi-Fi setup; UP/DOWN selects, click OK executes, long OK returns from subpages.
- **Main-screen long UP**: no shortcut. Holding UP for five seconds at boot still enters Recovery.
- **Setup click OK**: switch QR/credentials; UP/DOWN or long OK cancels to the network menu.

Choose favorites, active, or all with UP/DOWN and confirm to open its list. The
last opened category is remembered until reboot; favorites is selected initially.
Long OK returns from the list to categories, then from categories to monitoring.
Each entry reloads the first page. Favorites come from the connected FMO's
`station/getPinnedList`; active stations come from `station/getActiveList`, using
FMO's own activity definition and ordering. Both accept `start`/`count` and return
`uid`/`name` rows. Favorites are managed on FMO. Requests are serialized; a pending
request must finish or time out before another category can open, and replies
for other categories are ignored. Empty categories have distinct messages;
errors or unsupported/unanswered requests show a read failure with retry, never
an empty list. Browsing does not automatically reorder the displayed page;
a later page query may reflect changes in the remote list.

The station list uses the existing font, palette and single-line gray footer.
UP/DOWN selects among six rows and loads the adjacent page at the boundary.
Long UP/DOWN moves one page while preserving the selected row, clamped on
a short last page. Paging stops at either end and waits for pending requests;
the current station appears in the summary and has a right-side check mark in the list, matching the saved Wi-Fi list.
The upper-right indicator shows the selected station index/total, for example
`1/162`, `2/162`, or `3/162`, and follows selection across pages. It shows
`1/--` when the total is unavailable. Opening or returning to the first page
refreshes the total for all three categories with one-row range probes; other pages reuse it.
A conflicting page response invalidates or corrects a changed total. Counting
is limited to 32 probes and stops starting probes after three seconds, then
loads the ordinary page. The existing ten-second response timeout still applies.
Loading remains cancellable with long OK and playback continues. Selected long
names scroll; matching names on a page include their UIDs. Click OK to switch
immediately, without a confirmation page. Selecting the current station only
returns to monitoring with a hint. Long OK returns to categories without cancelling a sent
command. Browsing preserves playback; counting and page requests share the
control connection serially, then channel polling resumes.

A switch waits for the audio worker to mute and close its old stream, clears old
speech/grid/history, and restarts reception after the actual UID is confirmed.
Volume and mute preferences are preserved. Repeated OK presses cannot submit a
second command. Failed/empty lists support OK to reload; on an uncertain send,
disconnection or a ten-second response timeout, the firmware queries the actual
station without resending the switch. Audio remains paused while that result is
unresolved; long OK still returns so network setup remains accessible. There are
only six cached rows plus one transient lookahead row. This requires compatible
FMO `getListRange`, `getPinnedList`, `getActiveList`, `setCurrent` and `getCurrent` station APIs; physical switching,
continuity and memory/stack margins under live traffic require device validation.


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

Every boot automatically joins saved Wi-Fi and connects to FMO. Once Wi-Fi is
online, an event link or channel confirmation that remains unavailable for
30 seconds triggers a DNS cache refresh and replacement of only the failed
metadata clients. Failed attempts repeat at that interval without menu input.
Healthy connections and audio keep running; this does not switch Wi-Fi. Setup
and intentional station-switch waits suppress recovery. An ambiguous switch
only recovers the query connection to check the actual station, never resending
the switch command. Recovery cannot correct a wrong FMO address or provide
connectivity across an isolated LAN.

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
