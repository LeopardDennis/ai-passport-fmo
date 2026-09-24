<p align="right">
  <a href="CHANGELOG.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Changelog

## Unreleased

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
