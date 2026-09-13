#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage:
  app/benchmarks/thread_metric/run_f103rb.sh [options] [serial-port] [bench...]

Examples:
  app/benchmarks/thread_metric/run_f103rb.sh /dev/tty.usbmodem0007736309351
  app/benchmarks/thread_metric/run_f103rb.sh --duration-ms 5000 /dev/tty.usbmodem0007736309351 basic-processing message-processing

Options:
  -p, --port PORT       Serial device. If omitted, the script tries /dev/cu.usbmodem* then /dev/tty.usbmodem*.
  -d, --duration-ms N   Measurement window per cycle. Default: 30000.
  -c, --cycles N        Number of report cycles before PASS. Default: 1.
  -t, --timeout-sec N   Serial wait timeout per benchmark. Default: duration*cycles + 30 seconds.
      --log-dir DIR     Log directory. Default: build/thread-metric-f103rb-run-<timestamp>.
  -h, --help            Show this help.

Bench names:
  basic-processing cooperative-scheduling preemptive-scheduling
  interrupt-processing interrupt-preemption-processing message-processing
  synchronization-processing memory-allocation

Environment overrides are passed through to make/J-Link:
  JLINK JLINK_DEVICE JLINK_IF JLINK_SPEED F103RB_SYSCORECLK F103RB_SYSTICK_DIV
  F103RB_HSECLK F103RB_HSE_BYPASS F103RB_HSE_DIV2
USAGE
}

die() {
    echo "error: $*" >&2
    exit 1
}

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$repo_root"

duration_ms="${DURATION_MS:-30000}"
cycles="${CYCLES:-1}"
timeout_sec="${TIMEOUT_SEC:-}"
serial_port="${SERIAL_PORT:-}"
log_dir="${LOG_DIR:-}"
declare -a requested_benches=()

while (($# > 0)); do
    case "$1" in
        -p|--port)
            (($# >= 2)) || die "$1 needs a value"
            serial_port="$2"
            shift 2
            ;;
        -d|--duration-ms)
            (($# >= 2)) || die "$1 needs a value"
            duration_ms="$2"
            shift 2
            ;;
        -c|--cycles)
            (($# >= 2)) || die "$1 needs a value"
            cycles="$2"
            shift 2
            ;;
        -t|--timeout-sec)
            (($# >= 2)) || die "$1 needs a value"
            timeout_sec="$2"
            shift 2
            ;;
        --log-dir)
            (($# >= 2)) || die "$1 needs a value"
            log_dir="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            while (($# > 0)); do
                requested_benches+=("$1")
                shift
            done
            ;;
        -*)
            die "unknown option: $1"
            ;;
        *)
            if [[ -z "$serial_port" && "$1" == /dev/* ]]; then
                serial_port="$1"
            else
                requested_benches+=("$1")
            fi
            shift
            ;;
    esac
done

[[ "$duration_ms" =~ ^[0-9]+$ ]] || die "duration must be an integer"
[[ "$cycles" =~ ^[0-9]+$ ]] || die "cycles must be an integer"
((duration_ms > 0)) || die "duration must be greater than zero"
((cycles > 0)) || die "cycles must be greater than zero for the automated runner"

if [[ -z "$timeout_sec" ]]; then
    timeout_sec=$((((duration_ms * cycles + 999) / 1000) + 30))
fi
[[ "$timeout_sec" =~ ^[0-9]+$ ]] || die "timeout must be an integer"
((timeout_sec > 0)) || die "timeout must be greater than zero"

if [[ -z "$serial_port" ]]; then
    shopt -s nullglob
    cu_ports=(/dev/cu.usbmodem*)
    tty_ports=(/dev/tty.usbmodem*)
    shopt -u nullglob
    if ((${#cu_ports[@]} == 1)); then
        serial_port="${cu_ports[0]}"
    elif ((${#tty_ports[@]} == 1)); then
        serial_port="${tty_ports[0]}"
    else
        printf 'available /dev/*usbmodem* ports:\n' >&2
        printf '  %s\n' "${cu_ports[@]}" "${tty_ports[@]}" >&2
        die "pass the serial device with --port"
    fi
fi

[[ -e "$serial_port" ]] || die "serial device not found: $serial_port"

if ((${#requested_benches[@]} == 0)); then
    requested_benches=(
        basic-processing
        cooperative-scheduling
        preemptive-scheduling
        interrupt-processing
        interrupt-preemption-processing
        message-processing
        synchronization-processing
        memory-allocation
    )
fi

target_for_bench() {
    case "$1" in
        basic|basic-processing) echo "flash-thread-metric-basic-processing" ;;
        cooperative|cooperative-scheduling) echo "flash-thread-metric-cooperative-scheduling" ;;
        preemptive|preemptive-scheduling) echo "flash-thread-metric-preemptive-scheduling" ;;
        interrupt|interrupt-processing) echo "flash-thread-metric-interrupt-processing" ;;
        interrupt-preemption|interrupt-preemption-processing) echo "flash-thread-metric-interrupt-preemption-processing" ;;
        message|message-processing) echo "flash-thread-metric-message-processing" ;;
        synchronization|synchronization-processing) echo "flash-thread-metric-synchronization-processing" ;;
        memory|memory-allocation) echo "flash-thread-metric-memory-allocation" ;;
        *) return 1 ;;
    esac
}

pass_name_for_bench() {
    case "$1" in
        basic) echo "basic-processing" ;;
        cooperative) echo "cooperative-scheduling" ;;
        preemptive) echo "preemptive-scheduling" ;;
        interrupt) echo "interrupt-processing" ;;
        interrupt-preemption) echo "interrupt-preemption-processing" ;;
        message) echo "message-processing" ;;
        synchronization) echo "synchronization-processing" ;;
        memory) echo "memory-allocation" ;;
        *) echo "$1" ;;
    esac
}

configure_serial() {
    local port="$1"

    if stty -f "$port" 115200 cs8 -cstopb -parenb -ixon -ixoff -crtscts raw -echo 2>/dev/null; then
        return 0
    fi

    stty -F "$port" 115200 cs8 -cstopb -parenb -ixon -ixoff -crtscts raw -echo
}

drain_serial() {
    local port="$1"

    perl -MFcntl=O_RDONLY,O_NONBLOCK -MTime::HiRes=time,sleep -e '
        my ($port) = @ARGV;
        sysopen(my $fh, $port, O_RDONLY | O_NONBLOCK) or exit 0;
        my $end = time() + 0.5;
        while (time() < $end) {
            my $buf = "";
            sysread($fh, $buf, 4096);
            sleep 0.05;
        }
    ' "$port" || true
}

read_until_pass() {
    local port="$1"
    local log="$2"
    local pass_name="$3"
    local timeout="$4"

    perl -MErrno=EAGAIN,EWOULDBLOCK -MFcntl=O_RDONLY,O_NONBLOCK -MTime::HiRes=time,sleep -e '
        my ($port, $log, $pass_name, $timeout) = @ARGV;
        sysopen(my $fh, $port, O_RDONLY | O_NONBLOCK)
            or die "open serial $port: $!\n";
        my @stty_common = qw(115200 cs8 -cstopb -parenb -ixon -ixoff -crtscts raw -echo);
        my $stty_ok;
        if ($^O eq "darwin") {
            $stty_ok = (system("stty", "-f", $port, @stty_common) == 0);
        } else {
            $stty_ok = (system("stty", "-F", $port, @stty_common) == 0);
        }
        die "configure serial $port failed\n" unless $stty_ok;
        open(my $out, ">>", $log) or die "open log $log: $!\n";
        binmode($fh);
        binmode($out);
        binmode(STDOUT);
        select((select(STDOUT), $| = 1)[0]);
        select((select($out), $| = 1)[0]);

        my $start = time();
        my $tail = "";
        while ((time() - $start) < $timeout) {
            my $rin = "";
            vec($rin, fileno($fh), 1) = 1;
            my $rout = $rin;
            my $n = select($rout, undef, undef, 0.25);
            next if $n <= 0;

            my $buf = "";
            my $len = sysread($fh, $buf, 512);
            if (!defined $len) {
                next if $! == EAGAIN || $! == EWOULDBLOCK;
                die "serial read failed: $!\n";
            }
            next if $len == 0;

            print STDOUT $buf;
            print $out $buf;
            $tail .= $buf;
            $tail = substr($tail, -8192) if length($tail) > 16384;

            if ($tail =~ /TM PASS \Q$pass_name\E\b[^\r\n]*(?:\r?\n|\r)/) {
                exit 0;
            }
            if ($tail =~ /TM (FAIL|ERR)\b|HardFault|ASSERT|FAULT/) {
                exit 2;
            }
        }

        print STDERR "timeout waiting for TM PASS $pass_name\n";
        exit 124;
    ' "$port" "$log" "$pass_name" "$timeout"
}

append_bench_summary() {
    local status="$1"
    local pass_name="$2"
    local serial_log="${3:-}"
    local detail="${4:-}"

    {
        if [[ -n "$detail" ]]; then
            echo "$status $pass_name $detail"
        else
            echo "$status $pass_name"
        fi

        if [[ -n "$serial_log" && -s "$serial_log" ]]; then
            perl -0ne '
                s/\r\n?/\n/g;
                my @lines = split /\n/;
                my @block;
                my @last_block;
                my @markers;

                for my $line (@lines) {
                    next if $line eq "";

                    if ($line =~ /^\*\*\*\* RK0 Thread-Metric /) {
                        @block = ($line);
                        next;
                    }

                    if (@block &&
                        $line =~ /^(Time Period Total:|Total:|Counters:|Low thread:|Interrupts:)/) {
                        push @block, $line;
                        next;
                    }

                    if ($line =~ /^TM (PASS|FAIL|ERR) /) {
                        @last_block = @block if @block;
                        push @markers, $line;
                        @block = ();
                    }
                }

                print map { "    $_\n" } @last_block;
                print map { "    $_\n" } @markers;
            ' "$serial_log"
        fi
        echo
    } | tee -a "$summary"
}

timestamp="$(date '+%Y%m%d-%H%M%S')"
if [[ -z "$log_dir" ]]; then
    log_dir="build/thread-metric-f103rb-run-$timestamp"
fi
mkdir -p "$log_dir"

jlink="${JLINK:-JLinkExe}"
jlink_device="${JLINK_DEVICE:-STM32F103RB}"
jlink_if="${JLINK_IF:-SWD}"
jlink_speed="${JLINK_SPEED:-4000}"
command -v "$jlink" >/dev/null 2>&1 || die "J-Link Commander not found: $jlink"

tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/rk0-thread-metric.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT

reset_board() {
    local bench="$1"
    local script="$tmp_dir/reset.jlink"
    local log="$log_dir/$bench.jlink-reset.log"

    printf "r\ng\nq\n" > "$script"
    "$jlink" -device "$jlink_device" -if "$jlink_if" -speed "$jlink_speed" \
        -AutoConnect 1 -CommanderScript "$script" > "$log" 2>&1
}

make_vars=(
    "PLATFORM=stm32f103rb"
    "JLINK=$jlink"
    "JLINK_DEVICE=$jlink_device"
    "JLINK_IF=$jlink_if"
    "JLINK_SPEED=$jlink_speed"
)

for var in F103RB_SYSCORECLK F103RB_SYSTICK_DIV F103RB_HSECLK F103RB_HSE_BYPASS F103RB_HSE_DIV2; do
    if [[ -n "${!var:-}" ]]; then
        make_vars+=("$var=${!var}")
    fi
done

extra_defs="-DRK_THREAD_METRIC_CYCLES=${cycles}UL -DRK_THREAD_METRIC_TEST_DURATION_MS=${duration_ms}UL"

echo "RK0 Thread-Metric F103RB runner"
echo "  serial     : $serial_port"
echo "  duration   : ${duration_ms} ms"
echo "  cycles     : $cycles"
echo "  timeout    : ${timeout_sec} s per benchmark"
echo "  logs       : $log_dir"
echo
echo "Close screen/minicom before running this script; it needs the serial port."
echo

summary="$log_dir/summary.txt"
{
    echo "RK0 Thread-Metric F103RB summary"
    echo "serial=$serial_port duration_ms=$duration_ms cycles=$cycles timeout_sec=$timeout_sec"
    echo
} > "$summary"

for bench in "${requested_benches[@]}"; do
    pass_name="$(pass_name_for_bench "$bench")"
    target="$(target_for_bench "$pass_name")" || die "unknown benchmark: $bench"
    bench_log="$log_dir/$pass_name.serial.log"
    flash_log="$log_dir/$pass_name.flash.log"

    echo "==> $pass_name"
    echo "    flashing with $target"
    if ! make "$target" EXTRA_DEFS="$extra_defs" "${make_vars[@]}" > "$flash_log" 2>&1; then
        append_bench_summary "FAIL" "$pass_name" "" "flash"
        echo "flash log: $flash_log" >&2
        exit 1
    fi

    configure_serial "$serial_port"
    drain_serial "$serial_port"

    echo "    resetting and waiting for TM PASS $pass_name"
    reset_board "$pass_name"

    : > "$bench_log"
    if read_until_pass "$serial_port" "$bench_log" "$pass_name" "$timeout_sec"; then
        echo
        append_bench_summary "PASS" "$pass_name" "$bench_log"
    else
        rc=$?
        echo
        append_bench_summary "FAIL" "$pass_name" "$bench_log" "serial rc=$rc"
        echo "serial log: $bench_log" >&2
        echo "flash log : $flash_log" >&2
        exit "$rc"
    fi
    echo
done

echo "All requested Thread-Metric benchmarks passed."
echo "Summary: $summary"
