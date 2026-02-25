#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ARCH="${ARCH:-armv7m}"
BUILD="${BUILD:-DEBUG}"
TIMEOUT_SECS="${QEMU_TEST_TIMEOUT:-8}"

ALL_MODULES=(
    kversion
    kmem
    kmrm
    ksema
    kmutex
    kscheduler
    kmesgq
    kport
    ktaskevents
    ksleepq
    ktimer
    kasr
)

module_to_app_main() {
    case "$1" in
        kversion) echo "tests/qemu/apps/ut_kversion.c" ;;
        kmem) echo "tests/qemu/apps/ut_kmem.c" ;;
        kmrm) echo "tests/qemu/apps/ut_kmrm.c" ;;
        ksema) echo "tests/qemu/apps/ut_ksema.c" ;;
        kmutex) echo "tests/qemu/apps/ut_kmutex.c" ;;
        kscheduler) echo "tests/qemu/apps/ut_kscheduler.c" ;;
        kmesgq) echo "tests/qemu/apps/ut_kmesgq.c" ;;
        kport) echo "tests/qemu/apps/ut_kport.c" ;;
        ktaskevents) echo "tests/qemu/apps/ut_ktaskevents.c" ;;
        ksleepq) echo "tests/qemu/apps/ut_ksleepq.c" ;;
        ktimer) echo "tests/qemu/apps/ut_ktimer.c" ;;
        kasr) echo "tests/qemu/apps/ut_kasr.c" ;;
        *) return 1 ;;
    esac
}

CURRENT_QEMU_PID=""
RUN_REPORT_FILE="$(mktemp "${TMPDIR:-/tmp}/rk0-qemu-ut-report.XXXXXX")"

append_report() {
    printf "%s\n" "$*" >>"${RUN_REPORT_FILE}"
}

cleanup_qemu() {
    if [[ -n "${CURRENT_QEMU_PID}" ]]; then
        kill "${CURRENT_QEMU_PID}" >/dev/null 2>&1 || true
        wait "${CURRENT_QEMU_PID}" >/dev/null 2>&1 || true
        CURRENT_QEMU_PID=""
    fi
}

cleanup_all() {
    cleanup_qemu
    rm -f "${RUN_REPORT_FILE}" >/dev/null 2>&1 || true
}
trap cleanup_all EXIT INT TERM

case "${ARCH}" in
    armv7m)
        QEMU_MACHINE="lm3s6965evb"
        ;;
    armv6m)
        QEMU_MACHINE="microbit"
        ;;
    *)
        echo "[RK_UT] unsupported ARCH=${ARCH}. expected armv7m or armv6m."
        exit 2
        ;;
esac

if ! command -v qemu-system-arm >/dev/null 2>&1; then
    echo "[RK_UT] qemu-system-arm not found in PATH."
    exit 2
fi

if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    echo "[RK_UT] arm-none-eabi-gcc not found in PATH."
    exit 2
fi

SELECTED_MODULES=()
if [[ $# -eq 0 ]]; then
    SELECTED_MODULES=("${ALL_MODULES[@]}")
elif [[ "$1" == "--all" ]]; then
    if [[ $# -ne 1 ]]; then
        echo "[RK_UT] --all does not accept extra arguments."
        exit 2
    fi
    SELECTED_MODULES=("${ALL_MODULES[@]}")
else
    SELECTED_MODULES=("$@")
fi

for module in "${SELECTED_MODULES[@]}"; do
    if ! module_to_app_main "${module}" >/dev/null; then
        echo "[RK_UT] unknown module: ${module}"
        echo "[RK_UT] valid modules: ${ALL_MODULES[*]}"
        exit 2
    fi
done

RESULT_MODULES=()
RESULT_STATUS=()
CASE_TOTAL=0
CASE_PASSED=0
CASE_FAILED=0

record_result() {
    RESULT_MODULES+=("$1")
    RESULT_STATUS+=("$2")
}

run_module() {
    local module="$1"
    local module_upper
    local app_main
    local target
    local elf_path
    local build_log
    local log_dir
    local log_file
    local pass_marker
    local fail_marker
    local deadline
    local status=""
    local case_pass=0
    local case_fail=0

    module_upper="$(printf "%s" "${module}" | tr '[:lower:]' '[:upper:]')"
    app_main="$(module_to_app_main "${module}")"
    target="rk0_ut_${module}"
    build_log="$(mktemp "${TMPDIR:-/tmp}/rk0-qemu-ut-build-${module}.XXXXXX")"

    local test_defs="${EXTRA_DEFS:-} -DRK_QEMU_UNIT_TEST=1"

    if ! make -B -C "${ROOT_DIR}" ARCH="${ARCH}" BUILD="${BUILD}" TARGET="${target}" APP_MAIN="${app_main}" EXTRA_DEFS="${test_defs}" >"${build_log}" 2>&1; then
        append_report "[RK_UT] BUILD_FAIL module=${module}"
        cat "${build_log}" >>"${RUN_REPORT_FILE}"
        printf "\n" >>"${RUN_REPORT_FILE}"
        rm -f "${build_log}" >/dev/null 2>&1 || true
        record_result "${module}" "BUILD_FAIL"
        return
    fi
    rm -f "${build_log}" >/dev/null 2>&1 || true

    elf_path="${ROOT_DIR}/build/${ARCH}/${target}.elf"
    if [[ ! -f "${elf_path}" ]]; then
        append_report "[RK_UT] missing ELF: ${elf_path}"
        record_result "${module}" "BUILD_FAIL"
        return
    fi

    log_dir="${ROOT_DIR}/build/${ARCH}/qemu-tests"
    log_file="${log_dir}/${module}.log"
    mkdir -p "${log_dir}"
    : >"${log_file}"

    pass_marker="[RK_UT] MODULE RESULT: PASS ${module_upper}"
    fail_marker="[RK_UT] MODULE RESULT: FAIL ${module_upper}"

    append_report "[RK_UT] running module=${module} on QEMU (${QEMU_MACHINE}) ..."
    qemu-system-arm -machine "${QEMU_MACHINE}" -nographic -kernel "${elf_path}" >"${log_file}" 2>&1 &
    CURRENT_QEMU_PID="$!"

    deadline=$(( $(date +%s) + TIMEOUT_SECS ))
    while kill -0 "${CURRENT_QEMU_PID}" >/dev/null 2>&1; do
        if grep -Fq "${pass_marker}" "${log_file}"; then
            status="PASS"
            break
        fi
        if grep -Fq "${fail_marker}" "${log_file}"; then
            status="FAIL"
            break
        fi
        if (( $(date +%s) >= deadline )); then
            status="TIMEOUT"
            break
        fi
        sleep 0.1
    done

    if [[ -z "${status}" ]]; then
        if grep -Fq "${pass_marker}" "${log_file}"; then
            status="PASS"
        elif grep -Fq "${fail_marker}" "${log_file}"; then
            status="FAIL"
        else
            status="NO_MARKER"
        fi
    fi

    cleanup_qemu

    sed '/^qemu-system-arm: terminating on signal /d' "${log_file}" >>"${RUN_REPORT_FILE}"
    printf "\n" >>"${RUN_REPORT_FILE}"

    case_pass="$(grep -cE "\\[PASSED\\]" "${log_file}" || true)"
    case_fail="$(grep -cE "\\[FAILED\\]" "${log_file}" || true)"
    CASE_PASSED=$((CASE_PASSED + case_pass))
    CASE_FAILED=$((CASE_FAILED + case_fail))
    CASE_TOTAL=$((CASE_TOTAL + case_pass + case_fail))

    if [[ "${status}" == "PASS" ]]; then
        append_report "[RK_UT] PASS module=${module}"
    elif [[ "${status}" == "FAIL" ]]; then
        append_report "[RK_UT] FAIL module=${module}"
    elif [[ "${status}" == "TIMEOUT" ]]; then
        append_report "[RK_UT] TIMEOUT module=${module} (${TIMEOUT_SECS}s)"
    else
        append_report "[RK_UT] NO_MARKER module=${module}"
    fi

    record_result "${module}" "${status}"
}

for module in "${SELECTED_MODULES[@]}"; do
    run_module "${module}"
done

fail_count=0
for idx in "${!RESULT_MODULES[@]}"; do
    status="${RESULT_STATUS[$idx]}"
    if [[ "${status}" != "PASS" ]]; then
        fail_count=$((fail_count + 1))
    fi
done

modules_tested_upper=""
for module in "${SELECTED_MODULES[@]}"; do
    module_upper="$(printf "%s" "${module}" | tr '[:lower:]' '[:upper:]')"
    if [[ -n "${modules_tested_upper}" ]]; then
        modules_tested_upper+=", "
    fi
    modules_tested_upper+="${module_upper}"
done
append_report "MODULES TESTED: ${modules_tested_upper}"
append_report "NUMBER OF TESTS: ${CASE_TOTAL}"
append_report "PASS/FAIL: [${CASE_PASSED}/${CASE_FAILED}]"

cat "${RUN_REPORT_FILE}"

if (( fail_count > 0 )); then
    exit 1
fi

exit 0
