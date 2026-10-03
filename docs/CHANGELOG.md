<p align="right">
  <a href="CHANGELOG.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Changelog

## Unreleased

- Fix false channel synchronization after normal QSO history pushes by increasing bounded text assembly from 768 bytes to 2 KiB per socket. A live 20-entry history message was 953 bytes; the old overflow path cleared speech/channel state despite connected sockets. Preserve invalid-frame recovery and log only socket/frame sizes; verify production parsing with transport chunks, continuation frames, interleaved pings and subsequent PTT release.
- Replace local radio-configuration polling with the three newest FMO event-history contacts. The QSO panel shows white 14 px bold callsigns above full Beijing timestamps (YYYY-MM-DD HH:MM:SS) in regular gray text. Sort unsorted histories, retain repeated contacts and missing-slot placeholders, and keep history updates independent of live PTT/channel state. Render dates inside the panel to preserve the 24 KiB LVGL memory budget.
- Place speaking/last-contact status, the bold callsign and retained gray grid above QSO. Show cross-server speech in red for crossServer true/1, ordinary speech in orange and last-heard callsigns in white; clear stale flags on release, speaker changes or link/channel reset. Fit callsigns within the remaining width and center the yellow PCM bar between visible text and QSO, repainting both bar positions when its height changes. Balance panel padding and use one centered footer row with audio percentage (0% when muted) and the long-OK setup hint.

- Reduce premature reconnects during brief LAN delays: allow 10 seconds for a single channel query, 15 seconds for confirmed-channel freshness and 30 seconds for WebSocket pongs. Match control send waits to the 3-second transport timeout, retain delayed-reply ownership and log reconnect reasons. Keep radio panel, speech status and callsign rows fixed when data first arrives. Add delayed-response, hard-timeout and empty/populated-layout regressions. On-device reconnect recovery remains unverified.

- Replace the fixed speech underline with a yellow PCM RMS audio-level bar, using
  bounded integer calculation, quiet-input gating, an 80 ms full-scale rise
  and 500 ms fall. Update only the two bar rows at 20 Hz, using the existing
  panel without a separate widget. Reserve the QR canvas before small page
  objects to preserve contiguous redraw memory after animation. Clear
  stale/muted/offline levels and protect against a reconnect during codec I/O.
  Verify measurement, timing, playback lifecycle, native rendering/memory and
  an installable setup firmware image, including BLE size/partition protection.
  Physical playback and animation remain unverified.

- Balance monitor spacing with a larger visible gap below the connection status, consistent channel/profile separation, and tighter speaker/footer alignment. Keep audio state, volume and the long-OK setup hint together; show refresh requests in the connection row. Verify active speech, last-heard, initial idle, errors and native memory/layout without rebuilding firmware.

- Receive live conversation PCM from the FMO `/audio` WebSocket through the ES8311 speaker, with automatic 50% playback, bounded buffering, disconnect/mute cleanup, silent DMA priming and isolated failures. UP/DOWN adjusts volume, short OK toggles audio, long UP refreshes and long OK retains the network menu; show audio state/volume in the footer. Verify the live endpoint, PCM framing, worker lifecycle, controls and native UI; firmware and physical playback are not tested in this code-only update.

- Center the clock on the screen and reclaim the hidden grid row for wider radio-profile line spacing, moving the callsign closer to the footer. Retain space for real setup, idle and error hints; validate native layout without rebuilding firmware.

- Add a minute-resolution Beijing-time clock between FMO and battery. Synchronize asynchronously over SNTP after Wi-Fi connects, retain one service across reconnects, show `--:--` before valid time, and redraw the clock before wake-up. Verify minute/midnight transitions, unavailable time, reconnect initialization, UI layout and memory.

- Hide speaker grid coordinates in both active-speech and last-heard views; retain the four-row radio configuration panel, bold callsigns and connection/setup error hints.

- Add a portrait four-row radio configuration panel inspired by the supplied reference: device name, MHz frequency, antenna model and height in metres. Read local FMO physical configuration on connection and every 30 seconds; keep optional metadata independent of PTT/channel state, handle unset/invalid values and clear it on disconnect. Verify unit conversion, Chinese text, long rows, reconnects and native UI memory/layout.

- Keep an already confirmed channel visible when speech starts or the talker changes; refresh the channel in the background. Discard pre-transition channel replies without blanking the UI or extending freshness. Preserve synchronization for unknown channels, disconnects and query failures; add parser regressions for repeated starts, short PTT and late replies.

- Simplify monitor details to the grid without elapsed-time or station/host prefixes. Use licensed Montserrat Bold callsign glyphs at 32 px, with a 20 px bold fallback for long callsigns; retain regular setup-password text and verify native rendering.

- Keep the confirmed channel and last-heard display after explicit PTT release (`isSpeaking: false/0`), including empty, null or omitted callsigns. Ignore unused release metadata, retain named-release matching and reject invalid starts. Add parser/state/UI regression tests for idle transitions, duplicate releases and delayed replies.

- Fix setup AP access checks for IPv4-mapped IPv6 sockets so phones can open the setup page with the default dual-stack HTTP server. Keep station-interface requests blocked and add real-socket regression tests.

- Recreate the FMO control connection on query timeout/partial sends so delayed replies cannot inherit a newer query. Retain the network worker through startup failures, clean up partial Wi-Fi initialization and support immediate menu retry.
- Reject JSON-escaped NUL and overlong callsign/grid identifiers. Add controlled-clock query, startup-recovery and setup endpoint fault tests.
- Add saved FMO hostname/IPv4 and port settings to setup, plus a worker-owned DNS/TCP port check using saved Wi-Fi. Keep session-token/AP access restrictions and distinguish port reachability from API compatibility.
- Split the existing font losslessly into compact fallback fonts, preserving every existing glyph and bitmap without partition changes. Normalize only private temporary build configs and verify identical framebuffers and glyph coverage.

- Balance the visible spacing above and below the setup connection QR.

- Add a PDKPASS-style network menu (setup above retry), a Wi-Fi connection QR with click-OK credential view, immediate saved-network retry, and cancellable setup that restores only the current saved profiles. Cover controls, QR escaping, display wake behavior, and native rendering within the firmware memory budget.
- Reset all application data after each complete firmware installation, including
  reinstalling the same binary: clear saved Wi-Fi/FMO settings and legacy PDKPASS
  caches/reminders. Preserve identity/Recovery and same-installation reboot data.
  Package a reset request and commit the image marker only after cleanup succeeds;
  interrupted cleanup retries before networking. Verify the resource payload.

- Preserve Chinese channel names with validated UTF-8 and whole-character
  truncation. Discard malformed live messages safely and prevent stale channel
  replies from erasing a newer speaker.
- Dim the screen after 30 idle seconds and sleep the LCD after 90 seconds.
  Keep FMO networking and button scanning active; speech or a consumed first
  button gesture wakes and redraws the screen. Keep setup and continuous speech
  awake, pause battery polling while dark, and retry failed panel transitions.

- Move the setup hotspot to `192.168.9.1/24` to avoid common home-LAN overlap.
  Wait for the Wi-Fi station stop event before testing another credential so
  late events from an earlier attempt cannot validate the new one. Log LVGL
  pool usage at startup and check redraw headroom in the native UI test. The
  setup page now distinguishes authentication, missing network, security-mode
  and DHCP timeout failures. Add a macOS serial-log capture tool for device
  diagnostics without flashing or sending commands.

- Read battery SOC immediately on startup and then every two minutes instead
  of every 30 seconds; FMO status refresh timing is unchanged.

- Reduce the battery capsule from 44x22 to 32x16 pixels while retaining the
  internal percentage and vertically centered status-bar alignment.

- Translate monitor status and controls into Chinese and replace the BAT label
  with an iOS-inspired battery capsule containing the percentage (red at 20% or
  below, outlined when unavailable; charging is not inferred from SOC).

- Center visible text vertically in channel and callsign rows, including
  Chinese font padding, smaller long-callsign fonts, and setup transitions.

- Replace the monitor's pixel scenery with a black/orange/white FMO-inspired
  portrait UI, larger active-talker callsigns, Chinese channel glyphs, and a
  matching setup screen. Add native LVGL rendering tests and framebuffer previews.

- Save up to five Wi-Fi profiles with legacy migration, explicit deletion and
  bounded automatic failover; prefer the last successful network, retain healthy
  connections, and recreate FMO clients after Wi-Fi loss.

- Add password-protected phone Wi-Fi setup, nearby SSID selection, verified NVS
  persistence, long-OK reconfiguration, default fmo.local mDNS resolution, and
  credential-free installation builds via `./tools/validate.sh --setup`.

- Fix configured firmware packaging; separate dummy network validation artifacts.
- Recover clean WebSocket closes and failed client creation; publish complete state
  snapshots, invalidate stale channels, and clear callsigns after channel changes.
- Handle fragmented WebSocket messages; avoid unchanged label allocations, remove
  unused demo build inputs, and add heap/stack diagnostics and regression tests.

- Replaced the reference demo menu with an FMO live monitor application. It
  connects to the local FMO `/events` and `/ws` WebSocket endpoints, displays
  the selected channel and current or last-heard callsign, reports connection
  and battery state, supports backlight controls and manual channel refresh,
  and keeps Wi-Fi/FMO credentials out of tracked files through project Kconfig.

- Added the supplied 80-byte CW2017 profile for the specified 520 mAh cell, including content/update-flag checks, verified writes, the required restart sequence, and bounded SOC-readiness polling.

- Reorganized the documentation by function area with a dual entry point: the root `AGENTS.md` is now a thin router (hard constraints + task routing only) and the detailed AI workflow lives in `docs/development/ai-guide.md`; `agent-guide.md` was folded in. `docs/development/` gained a second level (`engineering/`, `ci/`, `release/`), and the `plays/` application archive and `experiences/` moved into a `docs/reference/` area with a dedicated README. Removed `docs/software-design/` (empty scaffold); folded the three `assets/{fonts,images,music}/README` leaves into the `assets/` README; flattened the six `project-completion` sub-documents into a single file; and unified each directory to a single README, eliminating every `INDEX` file and a duplicated experience index. All cross-references and bibliographic links were updated; no content was dropped.

- Made mini-program BLE install compatibility a template-level invariant: fixed
  protected `cardid`/Recovery partitions, retained the five-second UP-key
  Recovery boot hook, and added CI validation for merged-image structure,
  partition MD5/ranges, the 3 MB app limit, and protected payload exclusion.
- Documented a release-title convention for multi-app releases: name tags as `v<version>-<app-name>` (e.g. `v0.1.0-voice-keychain`) so the release title carries the version and the app, and confirm the title after the release is published so a release list is scannable by app.
- Added a post-release follow-up workflow: an `issue-suggestions` skill for filing user feedback as issues against the upstream project, an `experience-pr` skill for submitting reusable development experience as a documentation PR, a `docs/experiences/` directory for per-entry experience files, and supporting `project-completion`, `file-issues`, and experience-index documents.
- Simplified the tracked repository root: moved GitHub-recognized community documents into `.github/`, moved the changelog into `docs/`, updated every reference, and added a root-document allowlist to repository checks.
- Repository-wide language policy: every maintained Markdown default `.md` file is English, Simplified Chinese uses a paired `.zh_CN.md`, and both provide language switches. Static checks reject missing peers, missing switches, and Chinese prose in English defaults.
- Phase one of the AI development workflow: streamlined task-based context routing, unified local/CI validation, added PR checks and a template, and committed the dependency lock for reproducible builds.
- PR review fixes: pinned GitHub Actions to full commit SHAs, split build/release jobs by least privilege, disabled persisted sync checkout credentials, added Feature Request and Usage Question forms, clarified private security-report fallback, and corrected stale README, CI-trigger, and branch descriptions.
- Changed commit titles, PR titles, and PR bodies from Chinese-default to English; updated the Chinese punctuation rule so it no longer applies to PR descriptions.
- Reworked `build-firmware.yml` to pass `SDKCONFIG_DEFAULTS=sdkconfig.defaults`, enable `partitions.csv`, preserve the 8 MB image header, merge a flashable `FoloToy-AI-Passport-full.bin`, publish only that artifact, and use Actions cache v5.
- Integrated upstream PR #6 to resolve PR #4 conflicts: Wi-Fi, Bluetooth LE, radio lifecycle, and low-power demos; a 3 MB factory partition; build/menu/configuration updates; hardware-guide coverage; and bilingual capability tables.
- Defined English imperative Conventional Commit formatting for both commits and PR titles.
- Removed stale sync-workflow template comments and generalized an irrelevant Redis TTL rule to cache components.
- Added Chinese punctuation, credential safety, and recoverable file-deletion conventions.
- Expanded source-comment requirements for functions, state, ownership, concurrency, timing, registers, and magic values.
- Removed AI execution instructions from product READMEs so they remain human-facing product and repository overviews.
- Added `docs/development/agent-guide.md` as the focused AI workflow guide.
- Updated `AGENTS.md`, `docs/INDEX.md`, and the development index for the agent guide.
- Documented why the root README path is reserved for fork owners and how GitHub README precedence supports it.
- Created `main-update` from the upstream-aligned baseline and combined the repository-structure, firmware-CI, and upstream-sync work.
- Corrected the merged documentation index, workflow path, project tree, and CI references.
- Moved CI documentation from software design to `docs/development/`.
- Moved fork-only documentation assets from `assets/docs/` to `docs/assets/`.
- Moved the upstream English/Chinese project READMEs under `docs/` and renamed the documentation catalog to `docs/INDEX.md`.
- Initialized `AGENTS.md`, `CLAUDE.md`, and `CHANGELOG.md`.
- Standardized the initial project README language filenames.
- Added the `docs/`, `assets/`, and `skills/` directory structure.
- Moved the upstream hardware guide into `docs/hardware-design/`.
- Standardized subdirectory README capitalization and introduced fork conventions.
- Allowed fork-owned root README and supplemental documentation content on fork `main`.
- Added and documented the fork-only supplemental-document directory.
- Moved the build CI document to its dedicated CI branch before consolidation.
- Documented clean-`main` reasons, the direct-development exception, and Actions enablement for forks.
- Split the original agent rules into contribution, development, and fork documents with a compact root index.
- Updated software-design and project README references for the new documentation structure.
- Added the documentation catalog and task-triggered routing based on the earlier repository model.
- Added bilingual contribution, code-of-conduct, security, and support documents tailored to this ESP-IDF and fork workflow.
