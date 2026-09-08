#!/usr/bin/env bash
set -euo pipefail

mode="${1:---all}"
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    echo "Usage: $0 [--all|--static|--firmware|--configured]" >&2
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
        tests/test_fmo_monitor_state.c main/fmo_monitor_state.c \
        -o "${test_dir}/test_fmo_monitor_state"
    "${test_dir}/test_fmo_monitor_state"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_fmo_ws_rx.c main/fmo_ws_rx.c -o "${test_dir}/test_fmo_ws_rx"
    "${test_dir}/test_fmo_ws_rx"
    python3 tests/test_build_config.py
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
        config_defaults="${repo_root}/sdkconfig.defaults"
        output_dir="${repo_root}/build"
    fi

    SDKCONFIG_DEFAULTS="${config_defaults}" \
        idf.py -B "${validation_build_dir}" \
        -D "SDKCONFIG=${validation_build_dir}/sdkconfig" build
    idf.py -B "${validation_build_dir}" merge-bin \
        -o "${validation_build_dir}/FoloToy-AI-Passport-full.bin"
    python3 tools/verify_firmware.py "${validation_build_dir}"
    mkdir -p "${output_dir}"
    install -m 0644 \
        "${validation_build_dir}/FoloToy-AI-Passport-full.bin" \
        "${output_dir}/FoloToy-AI-Passport-full.bin"
    echo "Firmware build: PASS"
    if [[ "${mode}" != "--configured" ]]; then
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
    --firmware|--configured)
        run_firmware_checks
        ;;
    *)
        usage
        exit 2
        ;;
esac
