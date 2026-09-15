#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: benchmarks/run_benchmark.sh --port SERIAL_PORT [options] [bench...]

Options:
  --platform PLATFORM STM32 board platform: stm32f103rb or stm32f401re (default: stm32f103rb)
  --port PORT          Serial device, for example /dev/tty.usbmodem...
  --duration-ms MS     Benchmark period compiled into the image (default: 30000)
  --cycles N           PASS after N report periods (default: 1)
  --timeout-sec SEC    Serial wait timeout per benchmark (default: 60)
  --baud BAUD          Serial baud rate (default: 115200)
  --logs DIR           Log directory (default: build/thread-metric-<platform>-run-<stamp>)
  --flash-tool TOOL    Flash tool passed to make, for example jlink, openocd, st-flash
  --make MAKE          Make executable (default: make)
  --makefile FILE      Benchmark makefile (default: benchmarks/Makefile)

Bench names:
  basic-processing
  cooperative-scheduling
  preemptive-scheduling
  interrupt-processing
  interrupt-preemption-processing
  message-processing
  synchronization-processing
  memory-allocation
EOF
}

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

serial_port="${SERIAL_PORT:-}"
platform="${THREAD_METRIC_PLATFORM:-stm32f103rb}"
duration_ms="${DURATION_MS:-30000}"
cycles="${CYCLES:-1}"
timeout_sec="${TIMEOUT_SEC:-60}"
baud="${SERIAL_BAUD:-115200}"
log_dir=""
flash_tool="${FLASH_TOOL:-}"
make_cmd="${MAKE:-make}"
makefile="${THREAD_METRIC_MAKEFILE:-benchmarks/Makefile}"

default_benches=(
    basic-processing
    cooperative-scheduling
    preemptive-scheduling
    interrupt-processing
    interrupt-preemption-processing
    message-processing
    synchronization-processing
    memory-allocation
)
benches=()

while [ "$#" -gt 0 ]; do
    case "$1" in
        --platform)
            platform="${2:?missing --platform value}"
            shift 2
            ;;
        --port)
            serial_port="${2:?missing --port value}"
            shift 2
            ;;
        --duration-ms)
            duration_ms="${2:?missing --duration-ms value}"
            shift 2
            ;;
        --cycles)
            cycles="${2:?missing --cycles value}"
            shift 2
            ;;
        --timeout-sec)
            timeout_sec="${2:?missing --timeout-sec value}"
            shift 2
            ;;
        --baud)
            baud="${2:?missing --baud value}"
            shift 2
            ;;
        --logs)
            log_dir="${2:?missing --logs value}"
            shift 2
            ;;
        --flash-tool)
            flash_tool="${2:?missing --flash-tool value}"
            shift 2
            ;;
        --make)
            make_cmd="${2:?missing --make value}"
            shift 2
            ;;
        --makefile)
            makefile="${2:?missing --makefile value}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            benches+=("$@")
            break
            ;;
        -*)
            echo "unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
        *)
            benches+=("$1")
            shift
            ;;
    esac
done

if [ -z "$serial_port" ]; then
    echo "error: --port is required" >&2
    usage >&2
    exit 2
fi

case "$platform" in
    stm32f103rb|STM32F103RB)
        platform="stm32f103rb"
        board_label="F103RB"
        ;;
    stm32f401re|STM32F401RE|stm32f4014re|STM32F4014RE)
        platform="stm32f401re"
        board_label="F401RE"
        ;;
    *)
        echo "error: --platform must be stm32f103rb or stm32f401re" >&2
        exit 2
        ;;
esac

if [ "${#benches[@]}" -eq 0 ]; then
    benches=("${default_benches[@]}")
fi

case "$duration_ms" in
    ''|*[!0-9]*)
        echo "error: --duration-ms must be a positive integer" >&2
        exit 2
        ;;
esac

case "$cycles" in
    ''|*[!0-9]*)
        echo "error: --cycles must be a positive integer" >&2
        exit 2
        ;;
esac

case "$timeout_sec" in
    ''|*[!0-9]*)
        echo "error: --timeout-sec must be a positive integer" >&2
        exit 2
        ;;
esac

if [ "$duration_ms" -eq 0 ] || [ "$cycles" -eq 0 ] || [ "$timeout_sec" -eq 0 ]; then
    echo "error: duration, cycles, and timeout must be greater than zero" >&2
    exit 2
fi

if [ -z "$log_dir" ]; then
    log_dir="build/thread-metric-${platform}-run-$(date +%Y%m%d-%H%M%S)"
fi
mkdir -p "$log_dir"

defs="-DNDEBUG -U__KDEF_STACKOVFLW -DRK_CONF_TRACE=OFF -DRK_CONF_N_USRTASKS_MAX=6U"
defs="$defs -DRK_THREAD_METRIC_TEST_DURATION_MS=${duration_ms}UL"
defs="$defs -DRK_THREAD_METRIC_CYCLES=${cycles}UL"
summary_txt="$log_dir/summary.txt"
summary_tsv="$log_dir/summary.tsv"

{
    echo "RK0 Thread-Metric ${board_label} summary"
    echo "platform    : $platform"
    echo "serial      : $serial_port"
    echo "baud        : $baud"
    echo "duration_ms : $duration_ms"
    echo "cycles      : $cycles"
    echo "timeout_sec : $timeout_sec"
    echo
    printf "%-40s %-12s %s\n" "benchmark" "status" "results"
} >"$summary_txt"

printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" \
    "benchmark" "status" "period_total" "total" "average" "max_delta" \
    "errors" "irq_total" "irq_errors" "cycles" "tick_ms" \
    "RK_CONF_SYSCORECLK" "RK_gSysCoreClock" "reason" "rc" \
    "serial_log" "flash_log" >"$summary_tsv"

echo "RK0 Thread-Metric ${board_label} runner"
echo "  platform   : $platform"
echo "  serial     : $serial_port"
echo "  baud       : $baud"
echo "  duration   : $duration_ms ms"
echo "  cycles     : $cycles"
echo "  timeout    : $timeout_sec s per benchmark"
echo "  logs       : $log_dir"
echo "  makefile   : $makefile"
if [ -n "$flash_tool" ]; then
    echo "  flash tool : $flash_tool"
fi
echo
echo "Close screen/minicom before running this script; it needs the serial port."
echo

wait_for_pass() {
    local port="$1"
    local seconds="$2"
    local speed="$3"
    local pass_marker="$4"
    local fail_marker="$5"
    local serial_log="$6"

    python3 - "$port" "$seconds" "$speed" "$pass_marker" "$fail_marker" "$serial_log" <<'PY'
import os
import select
import sys
import termios
import time

port, timeout_s, baud_s, pass_marker, fail_marker, log_path = (
    sys.argv[1],
    int(sys.argv[2]),
    sys.argv[3],
    sys.argv[4],
    sys.argv[5],
    sys.argv[6],
)
deadline = time.monotonic() + timeout_s
pass_marker_bytes = pass_marker.encode("ascii", "replace")
fail_marker_bytes = fail_marker.encode("ascii", "replace")
buf = bytearray()
pass_marker_pos = -1
fail_marker_pos = -1

def speed_constant(baud):
    name = "B" + baud
    if not hasattr(termios, name):
        raise SystemExit(f"unsupported baud rate for termios: {baud}")
    return getattr(termios, name)

def configure_fd(fd, baud):
    attrs = termios.tcgetattr(fd)
    attrs[0] &= ~(
        termios.IGNBRK
        | termios.BRKINT
        | termios.PARMRK
        | termios.ISTRIP
        | termios.INLCR
        | termios.IGNCR
        | termios.ICRNL
        | termios.IXON
        | termios.IXOFF
    )
    attrs[1] &= ~termios.OPOST
    attrs[2] &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB)
    attrs[2] |= termios.CS8 | termios.CREAD | termios.CLOCAL
    if hasattr(termios, "CRTSCTS"):
        attrs[2] &= ~termios.CRTSCTS
    attrs[3] &= ~(termios.ECHO | termios.ECHONL | termios.ICANON |
                  termios.ISIG | termios.IEXTEN)
    attrs[4] = speed_constant(baud)
    attrs[5] = speed_constant(baud)
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)

fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
try:
    configure_fd(fd, baud_s)
    try:
        termios.tcflush(fd, termios.TCIFLUSH)
    except termios.error:
        pass

    with open(log_path, "ab", buffering=0) as log:
        while time.monotonic() < deadline:
            remaining = max(0.0, deadline - time.monotonic())
            readable, _, _ = select.select([fd], [], [], min(0.25, remaining))
            if not readable:
                continue

            try:
                chunk = os.read(fd, 4096)
            except BlockingIOError:
                continue

            if not chunk:
                continue

            log.write(chunk)
            os.write(1, chunk)
            buf.extend(chunk)
            if pass_marker_pos < 0:
                pass_marker_pos = buf.find(pass_marker_bytes)
            if pass_marker_pos >= 0 and b"\n" in buf[pass_marker_pos:]:
                sys.exit(0)
            if fail_marker_pos < 0:
                fail_marker_pos = buf.find(fail_marker_bytes)
            if fail_marker_pos >= 0 and b"\n" in buf[fail_marker_pos:]:
                sys.exit(1)

            if len(buf) > 8192:
                keep = 4096
                drop = len(buf) - keep
                del buf[:drop]
                if pass_marker_pos >= 0:
                    pass_marker_pos = max(0, pass_marker_pos - drop)
                if fail_marker_pos >= 0:
                    fail_marker_pos = max(0, fail_marker_pos - drop)

    sys.exit(124)
finally:
    os.close(fd)
PY
}

append_result_summary() {
    local bench="$1"
    local serial_log="$2"
    local flash_log="$3"
    local status="$4"

    awk -v bench="$bench" \
        -v status_in="$status" \
        -v serial_log="$serial_log" \
        -v flash_log="$flash_log" \
        -v summary_txt="$summary_txt" \
        -v summary_tsv="$summary_tsv" '
        function value(line, key, parts, n, i, prefix) {
            prefix = key "="
            n = split(line, parts, /[[:space:]]+/)
            for (i = 1; i <= n; i++) {
                if (index(parts[i], prefix) == 1) {
                    return substr(parts[i], length(prefix) + 1)
                }
            }
            return ""
        }
        {
            line = $0
            gsub(/\r/, "", line)

            if (line ~ /Time Period Total:/) {
                n = split(line, parts, /[[:space:]]+/)
                period_total = parts[n]
            } else if (line ~ /^Total:/) {
                total = $2
                errors = value(line, "errors")
                tick_ms = value(line, "tick_ms")
                syscoreclk = value(line, "RK_CONF_SYSCORECLK")
                gsyscoreclk = value(line, "RK_gSysCoreClock")
            } else if (line ~ /^Counters:/) {
                total = value(line, "total")
                average = value(line, "average")
                max_delta = value(line, "max_delta")
                errors = value(line, "errors")
                tick_ms = value(line, "tick_ms")
                syscoreclk = value(line, "RK_CONF_SYSCORECLK")
                gsyscoreclk = value(line, "RK_gSysCoreClock")
            } else if (line ~ /^Interrupts:/) {
                irq_total = value(line, "total")
                irq_errors = value(line, "irq_errors")
            } else if (line ~ /TM PASS/) {
                status = "PASS"
                cycles = value(line, "cycles")
                pass_errors = value(line, "errors")
            } else if (line ~ /TM FAIL/) {
                status = "FAIL"
                fail_cycles = value(line, "cycles")
                fail_errors = value(line, "errors")
            }
        }
        END {
            if (status == "") {
                status = status_in
            }
            if (errors == "" && pass_errors != "") {
                errors = pass_errors
            }
            if (errors == "" && fail_errors != "") {
                errors = fail_errors
            }
            if (cycles == "" && fail_cycles != "") {
                cycles = fail_cycles
            }
            if ((errors != "") && ((errors + 0) != 0)) {
                status = "FAIL"
            }

            display = sprintf("%-40s %-12s", bench, status)
            if (period_total != "") {
                display = display " period=" period_total
            }
            if (total != "") {
                display = display " total=" total
            }
            if (average != "") {
                display = display " average=" average
            }
            if (max_delta != "") {
                display = display " max_delta=" max_delta
            }
            if (irq_total != "") {
                display = display " irq_total=" irq_total
            }
            if (errors != "") {
                display = display " errors=" errors
            }
            if (irq_errors != "") {
                display = display " irq_errors=" irq_errors
            }
            if (cycles != "") {
                display = display " cycles=" cycles
            }
            if (tick_ms != "") {
                display = display " tick_ms=" tick_ms
            }
            if (syscoreclk != "") {
                display = display " RK_CONF_SYSCORECLK=" syscoreclk
            }
            if (gsyscoreclk != "") {
                display = display " RK_gSysCoreClock=" gsyscoreclk
            }

            print "    " display
            print display >> summary_txt
            printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t\t\t%s\t%s\n", \
                bench, status, period_total, total, average, max_delta, errors, \
                irq_total, irq_errors, cycles, tick_ms, syscoreclk, \
                gsyscoreclk, serial_log, flash_log >> summary_tsv
        }
    ' "$serial_log"
}

append_failure_summary() {
    local bench="$1"
    local status="$2"
    local reason="$3"
    local rc="$4"
    local serial_log="$5"
    local flash_log="$6"

    local display
    display=$(printf "%-40s %-12s reason=%s rc=%s" \
        "$bench" "$status" "$reason" "$rc")
    echo "    $display"
    echo "$display" >>"$summary_txt"
    printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" \
        "$bench" "$status" "" "" "" "" "" "" "" "" "" "" "" \
        "$reason" "$rc" "$serial_log" "$flash_log" >>"$summary_tsv"
}

for bench in "${benches[@]}"; do
    target="flash-thread-metric-${bench}"
    flash_log="$log_dir/${bench}.flash.log"
    serial_log="$log_dir/${bench}.serial.log"
    pass_marker="TM PASS ${bench}"
    fail_marker="TM FAIL ${bench} cycles="

    echo "==> $bench"
    echo "    flashing with $target"

    set +e
    make_args=(
        --no-print-directory
        -f "$makefile"
        PLATFORM="$platform"
        THREAD_METRIC_DEFS="$defs"
    )
    if [ -n "$flash_tool" ]; then
        make_args+=(FLASH_TOOL="$flash_tool")
    fi
    "$make_cmd" "${make_args[@]}" "$target" >"$flash_log" 2>&1
    flash_rc=$?
    set -e

    if [ "$flash_rc" -ne 0 ]; then
        append_failure_summary "$bench" "FLASH_FAIL" "flash" "$flash_rc" \
            "$serial_log" "$flash_log"
        echo "FAIL $bench flash"
        echo "flash log: $flash_log"
        echo "summary  : $summary_txt"
        exit "$flash_rc"
    fi

    : >"$serial_log"

    echo "    resetting and waiting for $pass_marker"
    set +e
    wait_for_pass "$serial_port" "$timeout_sec" "$baud" \
        "$pass_marker" "$fail_marker" "$serial_log"
    serial_rc=$?
    set -e

    if [ "$serial_rc" -eq 1 ]; then
        echo
        append_result_summary "$bench" "$serial_log" "$flash_log" "FAIL"
        echo "FAIL $bench benchmark errors"
        echo "serial log: $serial_log"
        echo "flash log : $flash_log"
        echo "summary   : $summary_txt"
        exit 1
    fi

    if [ "$serial_rc" -ne 0 ]; then
        append_failure_summary "$bench" "SERIAL_FAIL" "serial" "$serial_rc" \
            "$serial_log" "$flash_log"
        echo
        echo "timeout waiting for $pass_marker"
        echo
        echo "FAIL $bench serial rc=$serial_rc"
        echo "serial log: $serial_log"
        echo "flash log : $flash_log"
        echo "summary   : $summary_txt"
        exit "$serial_rc"
    fi

    echo
    append_result_summary "$bench" "$serial_log" "$flash_log" "PASS"
    echo "PASS $bench"
    echo
done

echo "Thread-Metric summary:"
cat "$summary_txt"
echo
echo "Thread-Metric summary logs: $log_dir"
echo "  text : $summary_txt"
echo "  tsv  : $summary_tsv"
