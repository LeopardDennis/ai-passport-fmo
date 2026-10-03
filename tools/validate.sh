#!/usr/bin/env bash
set -euo pipefail

mode="${1:---all}"
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    echo "Usage: $0 [--all|--static|--firmware|--configured|--setup]" >&2
}

run_static_checks() {
    local actionlint_bin
    local test_dir

    python3 tools/check_repo.py

    actionlint_bin="${ACTIONLINT_BIN:-}"
    if [[ -z "${actionlint_bin}" ]]; then
        actionlint_bin="$(command -v actionlint || true)"
    fi
    if [[ -z "${actionlint_bin}" || ! -x "${actionlint_bin}" ]]; then
        actionlint_bin="$(./tools/install-actionlint.sh)"
    fi
    "${actionlint_bin}" -color .github/workflows/*.yml

    test_dir="$(mktemp -d /tmp/ai-passport-host-tests.XXXXXX)"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_ui_pixel_math.c main/ui_pixel_math.c \
        -o "${test_dir}/test_ui_pixel_math"
    "${test_dir}/test_ui_pixel_math"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_fmo_monitor_state.c main/fmo_monitor_state.c main/fmo_text.c \
        -o "${test_dir}/test_fmo_monitor_state"
    "${test_dir}/test_fmo_monitor_state"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_fmo_ws_rx.c main/fmo_ws_rx.c -o "${test_dir}/test_fmo_ws_rx"
    "${test_dir}/test_fmo_ws_rx"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_fmo_text.c main/fmo_text.c main/fmo_monitor_state.c -o "${test_dir}/test_fmo_text"
    "${test_dir}/test_fmo_text"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_fmo_display_policy.c main/fmo_display_policy.c -o "${test_dir}/test_fmo_display_policy"
    "${test_dir}/test_fmo_display_policy"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_fmo_storage_policy.c main/fmo_storage_policy.c -o "${test_dir}/test_fmo_storage_policy"
    "${test_dir}/test_fmo_storage_policy"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_fmo_controls.c main/fmo_controls.c main/fmo_wifi_qr.c -o "${test_dir}/test_fmo_controls"
    "${test_dir}/test_fmo_controls"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_fmo_endpoint.c main/fmo_endpoint.c -o "${test_dir}/test_fmo_endpoint"
    "${test_dir}/test_fmo_endpoint"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_fmo_clock.c main/fmo_clock.c -o "${test_dir}/test_fmo_clock"
    "${test_dir}/test_fmo_clock"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_fmo_pcm.c main/fmo_pcm.c -o "${test_dir}/test_fmo_pcm"
    "${test_dir}/test_fmo_pcm"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_fmo_audio_meter.c main/fmo_audio_meter.c -o "${test_dir}/test_fmo_audio_meter"
    "${test_dir}/test_fmo_audio_meter"
    python3 tests/test_fmo_audio_runtime.py
    python3 tests/test_compact_font.py
    python3 tests/test_fmo_storage.py
    python3 tests/test_fmo_display_runtime.py
    python3 tests/test_fmo_provision_access.py
    python3 tests/test_fmo_provision_runtime.py
    python3 tests/test_fmo_network.py
    python3 tests/test_fmo_endpoint_runtime.py
    python3 tests/test_build_config.py
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_fmo_credentials.c main/fmo_credentials.c -o "${test_dir}/test_fmo_credentials"
    "${test_dir}/test_fmo_credentials"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_fmo_wifi_profiles.c main/fmo_wifi_profiles.c main/fmo_credentials.c \
        -o "${test_dir}/test_fmo_wifi_profiles"
    "${test_dir}/test_fmo_wifi_profiles"
    node tests/test_fmo_provision_page.cjs
    python3 tests/test_verify_firmware.py
    rm -rf "${test_dir}"
    echo "Host tests: PASS"
}

run_firmware_checks() (
    local validation_build_dir
    local config_defaults="${repo_root}/sdkconfig.defaults;${repo_root}/tests/sdkconfig.network"
    local output_dir="${repo_root}/build/validation"

    if ! command -v idf.py >/dev/null 2>&1; then
        echo "ERROR: idf.py is not available; activate ESP-IDF 5.5.3 first." >&2
        return 1
    fi

    validation_build_dir="$(mktemp -d /tmp/ai-passport-firmware.XXXXXX)"
    trap 'case "${validation_build_dir}" in /tmp/ai-passport-firmware.*) rm -rf -- "${validation_build_dir}" ;; esac' EXIT

    if [[ "${mode}" == "--configured" ]]; then
        python3 tools/check_fmo_config.py "${repo_root}/sdkconfig"
        install -m 0600 "${repo_root}/sdkconfig" "${validation_build_dir}/sdkconfig"
        # The compact font ABI requires the small descriptor in every mode.
        # Normalize only the private temporary copy; retain the user's config.
        python3 - "${validation_build_dir}/sdkconfig" <<'PYCONFIG'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text().replace("CONFIG_LV_FONT_FMT_TXT_LARGE=y",
                                "# CONFIG_LV_FONT_FMT_TXT_LARGE is not set")
path.write_text(text)
PYCONFIG
        config_defaults="${repo_root}/sdkconfig.defaults"
        output_dir="${repo_root}/build"
    fi
    if [[ "${mode}" == "--setup" ]]; then
        config_defaults="${repo_root}/sdkconfig.defaults"
        output_dir="${repo_root}/build"
    fi

    # Validate pinned dependencies without resolving optional newer versions.
    IDF_COMPONENT_CHECK_NEW_VERSION=0 SDKCONFIG_DEFAULTS="${config_defaults}" \
        idf.py -B "${validation_build_dir}" \
        -D "SDKCONFIG=${validation_build_dir}/sdkconfig" build
    FMO_REQUIRE_NETWORK_TEST=1 python3 tests/test_fmo_network.py
    FMO_REQUIRE_NETWORK_TEST=1 python3 tests/test_fmo_endpoint_runtime.py
    idf.py -B "${validation_build_dir}" merge-bin \
        -o "${validation_build_dir}/FoloToy-AI-Passport-full.bin"
    python3 tools/verify_firmware.py "${validation_build_dir}"
    # The native preview uses the production UI and the same LVGL pool budget.
    cmake -S tests/ui_preview -B "${validation_build_dir}/ui_preview" \
        >"${validation_build_dir}/ui_configure.log"
    cmake --build "${validation_build_dir}/ui_preview" -j 2 \
        >"${validation_build_dir}/ui_build.log"
    ctest --test-dir "${validation_build_dir}/ui_preview" --output-on-failure
    mkdir -p "${output_dir}"
    install -m 0644 \
        "${validation_build_dir}/FoloToy-AI-Passport-full.bin" \
        "${output_dir}/FoloToy-AI-Passport-full.bin"
    echo "Firmware build: PASS"
    if [[ "${mode}" != "--configured" && "${mode}" != "--setup" ]]; then
        echo "Validation image uses dummy network settings; use --configured for your device."
    fi
)

cd "${repo_root}"
case "${mode}" in
    --all)
        run_static_checks
        run_firmware_checks
        ;;
    --static)
        run_static_checks
        ;;
    --firmware|--configured|--setup)
        run_firmware_checks
        ;;
    *)
        usage
        exit 2
        ;;
esac
