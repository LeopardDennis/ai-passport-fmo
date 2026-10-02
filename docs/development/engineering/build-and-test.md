<p align="right">
  <a href="build-and-test.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

> FMO fork: `--firmware` uses public dummy network settings and writes only
> `build/validation/FoloToy-AI-Passport-full.bin`. Installable private images use
> `idf.py menuconfig` followed by `./tools/validate.sh --configured` and retain
> the `build/FoloToy-AI-Passport-full.bin` path. CI uploads validation artifacts
> only; automatic release publishing is disabled. See the root README.
> Credential-free installable builds use `./tools/validate.sh --setup`.
> The compact font keeps every existing glyph and requires
> `CONFIG_LV_FONT_FMT_TXT_LARGE=n`; configured builds normalize their temporary
> config copy, without changing the original.

# Build and Test

Use ESP-IDF 5.5.3. On a clean machine or when the toolchain is missing, follow
the [environment bootstrap](environment-setup.md) first.

> Use `./tools/validate.sh --setup` for installable firmware and flash its
> verified `build/FoloToy-AI-Passport-full.bin` at offset `0x0` only when the
> target is blank or the merged byte range ends before protected `cardid`.
> On a provisioned device, prefer mini-program install or segmented
> `idf.py flash`. Treat
> `idf.py build` and `idf.py flash` as incremental development commands, not the
> default delivery path.

```bash
source <path-to-esp-idf-v5.5.3>/export.sh
idf.py --version             # must report ESP-IDF v5.5.3
./tools/validate.sh --setup    # build and verify installable merged image
idf.py set-target esp32c3     # fresh checkout or changed target
idf.py build                  # optional incremental application build
idf.py flash monitor          # optional incremental application flash
idf.py fullclean              # remove stale generated build state only
```

`idf.py fullclean` does not fully synchronize an existing `sdkconfig` with
changed defaults. Preserve intentional local settings, then run
`idf.py set-target esp32c3` when the target or tracked defaults must be
regenerated.

The tracked `dependencies.lock` pins Managed Component resolution. After changing an `idf_component.yml`, regenerate the lock with ESP-IDF 5.5.3, review version changes, and commit it with the manifest. An ordinary build must not leave an unexplained lock-file diff.

Firmware validation uses a fresh temporary build directory and an isolated `sdkconfig` generated from the tracked defaults. It does not consume or overwrite a developer's root `sdkconfig`, and it copies the verified validation image to `build/validation/FoloToy-AI-Passport-full.bin`. Installable `--setup`/`--configured` images go to `build/FoloToy-AI-Passport-full.bin`. The gate also enforces the [mini-program BLE compatibility contract](ble-recovery-compatibility.md): protected partition addresses, application size, partition-table MD5, absence of protected payload data, and the Recovery bootloader hook.

The baseline also has a hardware-independent logic test:

```bash
cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_ui_pixel_math.c main/ui_pixel_math.c \
  -o /tmp/test_ui_pixel_math
/tmp/test_ui_pixel_math
```

Use the unified validation entry point:

```bash
./tools/validate.sh --static    # repository checks, workflows, links, secrets, host tests (including Node.js setup-page test)
./tools/validate.sh --firmware  # build, merge-bin, offsets, BLE compatibility, native UI tests
./tools/validate.sh             # complete gate; requires an activated ESP-IDF environment
```

CI calls the same script. Fix the shared script or environment if local and CI behavior differs; do not duplicate command sequences in workflows.

Hardware-affecting changes must also run the applicable on-device checklist in the hardware guide. Report compilation separately from physical-device validation.

The firmware gate tests the production network parser using ESP-IDF's cJSON;
`--static` skips that test when ESP-IDF is inactive. Host checks cover UTF-8
boundaries and LCD transition/gesture ordering with hardware stubs. Dependency
checks use the committed lock without checking optional newer component versions.
These tests do not replace physical wake-up and current-consumption checks.

Installation-reset host tests simulate fresh installation, same-binary reinstall,
ordinary reboot, changed images, interrupted cleanup/marker writes and invalid
layouts. They verify protected bytes stay intact and cleanup precedes networking.
The merged-image check requires the fresh-install resource payload; on-device
installation/reboot acceptance is still required.

## macOS device log capture

The capture tool needs only Python 3. Start it before rebooting the Passport so
the boot and reconnect sequence is included:

```bash
python3 tools/device-test/serial_capture.py --list-ports
python3 tools/device-test/serial_capture.py --seconds 300
# If multiple USB modems are present, specify the device:
python3 tools/device-test/serial_capture.py --port /dev/cu.usbmodemXXXX --seconds 300
```

It does not flash or send commands. Raw logs are stored with private permissions
under `/tmp/fmo-device-logs/`; the terminal summarizes boot, UI memory, Wi-Fi,
WebSocket, and error events. Inspect and redact raw logs before sharing them.

Never upload the app-only `build/FoloToy-AI-Passport.bin` to the community. Only
the validated `build/FoloToy-AI-Passport-full.bin` contains the structure the
mini-program can inspect and transform safely.

The complete gate also covers channel-query timeout/late replies, retry after
partial startup failure, endpoint validation/persistence and DNS/TCP probe
outcomes with injected system-call results. Native UI tests include the font's
fallback range; host font checks compare every glyph metric and compressed byte.
Physical LAN reachability and current consumption still require device tests.
