# Resource-sharing experiments on NUCLEO-F103RB

E1, E2 and E3 exercise RK0 resource-sharing contracts and measure **elapsed CPU
clock cycles with the Cortex-M3 DWT_CYCCNT counter**. They do not measure SysTick
ticks and do not convert the earlier QEMU counts into hardware cycles.

This is a new hardware suite implementing the documented experimental scenarios.
It is not a byte-for-byte port of the private QEMU harness. Keep its hardware
dataset, build configuration and measurement definitions with the results.

## Build and collect

Requirements: ARM GNU Toolchain, GNU make, Python 3.10 or later, PySerial, a
NUCLEO-F103RB and a supported programmer. The default programmer is OpenOCD with
the board's built-in ST-LINK; the existing RK0 build also supports `st-flash`,
J-Link and STM32CubeProgrammer. No kernel changes or vendor HAL are required.

From the RK0 repository root:

```sh
python3 -m pip install pyserial
make -f benchmarks/resource_sharing/Makefile build
make -f benchmarks/resource_sharing/Makefile run SERIAL_PORT=/dev/ttyACM0
```

On macOS use the board's `/dev/cu.usbmodem...` device. On Windows use a supported
GNU make environment such as MSYS2, or WSL with USB forwarded; use the serial
device visible to that environment. Native Windows Python normally uses `COM5`
or a similar name. Close other terminal/serial-monitor applications first.

Select experiments or change the sample count:

```sh
make -f benchmarks/resource_sharing/Makefile run \
    BENCHES='e1 e2' SAMPLES=20 SERIAL_PORT=/dev/ttyACM0 FLASH_TOOL=openocd
```

The runner builds each image, opens serial **before** flashing, records output
until the experiment's PASS marker, then validates the complete case/metric
matrix. It stops on failure and retains diagnostics. It does not continue with
a partial dataset or substitute timing results. OpenOCD uses ELF; the other
supported programmers use BIN. Each programming command verifies its image.

The default clock is 72 MHz with an 8 MHz HSE input in bypass mode, using RK0's
existing board initialisation. This assumes the Nucleo's ST-LINK MCO connection
is configured appropriately. Check the board revision and solder bridges in
UM1724 if it does not boot. `CORE_HZ=8000000` selects the existing HSI path for a
separate 8 MHz run. Never mix clock configurations in one comparison. The UART
is USART2, 115200 baud, available through the ST-LINK virtual COM port.

For manual programming and capture:

```sh
make -f benchmarks/resource_sharing/Makefile flash-e2 FLASH_TOOL=openocd
python3 benchmarks/resource_sharing/run.py --capture-only --port /dev/ttyACM0 e2
# Press RESET after the runner says the serial port is ready.
```

The default output directory is `build/resource-sharing-<UTC timestamp>/`.
`LOG_DIR=<new-directory>` selects another directory. Existing output directories
are not overwritten. Files include:

| File | Meaning |
| --- | --- |
| `e*.serial.log` | Complete raw UART output, including contract checks and run configuration |
| `e*.build.log` | Compiler/linker commands and image memory usage |
| `e*.flash.log` | Programming and verification output |
| `manifest.json` | Host command, source revision/status, compiler, image hashes and run outcome |
| `capture_metadata.json` | Firmware metadata and hashes of the source logs |
| `results.csv` | Every raw measurement; cycles are unsigned integers |
| `summary.csv` | Sample count, minimum, median, mean, maximum and standard deviation |

Reprocess existing logs without a board or PySerial:

```sh
python3 benchmarks/resource_sharing/run.py \
    --input path/to/e1.serial.log path/to/e2.serial.log path/to/e3.serial.log
```

Default run: five measured samples and one discarded warm-up **per configuration**,
256 operations per batch, `PROFILE` (`-O2`), 1 kHz SysTick, trace and API argument
checking disabled. Experiment assertions remain active. There are 625 timing
records: E1 215, E2 280, E3 130, plus separate semantic checks.

## Timing contract

Firmware enables DEMCR.TRCENA and DWT_CTRL.CYCCNTENA, checks that CYCCNT exists,
checks the enable bit and verifies that the counter advances. Failure aborts;
there is **no tick-based fallback**. The counter is reset once at startup and
runs freely thereafter, allowing timestamps from different tasks to share the
same timebase. Reads use volatile register access and compiler barriers. DSB/ISB
are used at initialisation, not added to every measured read.

`(uint32_t)(end - start)` handles one counter wrap provided the actual interval
is shorter than 2^32 cycles: about 59.65 seconds at 72 MHz. The supplied workloads
and two-second coordination timeouts keep individual measurements well below
that limit. Do not increase workloads or timeouts past it. A blocked or faulty
run is rejected by firmware checks and the host capture deadline.

The measured values are **elapsed cycles between defined events**. Interrupts,
preemption, scheduling, coordination and instrumentation inside an interval are
part of its value. They are not exclusive CPU consumption by one task. SysTick
continues to provide kernel timeouts. A benchmark-only priority-30 task keeps
the processor awake, avoiding WFI/sleep clock-gating ambiguity without modifying
the kernel's idle task. No other application workload is present.

Run continuously without breakpoints, single-stepping, semihosting, or live
debugger polling during trials. CYCCNT does not count halted debug execution.
Normal serial output occurs after the measured work. The boot record includes
the minimum of 256 consecutive timestamp pairs. This calibration is reported
but **not subtracted** from measurements. Flash configuration and RCC clock
configuration registers, build mode, optimisation, sample and batch sizes are
also recorded. `core_hz` is the frequency configured by firmware, not an
independent frequency-counter measurement.

## E1: serialised resource work

Priorities: controller 1, H 3, M 5, L 10, invocation server 12. Smaller numbers
mean greater urgency. L enters the resource operation before the controller
releases interference. Both mutex policies execute the same resource payload.

| Cases | Contract |
| --- | --- |
| `contended-pip`, `contended-ceiling` | H and M are released while L owns the mutex. Ceiling is 3. |
| `single-pip`, `single-ceiling`, `single-invocation` | Only one request is outstanding. M is released during L's resource work; H is released after L and M complete. |
| `batch-pip`, `batch-ceiling`, `batch-invocation` | 256 complete, uncontended transactions, each incrementing a shared word; no NOP payload. |

No application queue is added to make invocation accept concurrent requests.
The service checks serialisation, returned values, effective priorities and
restoration. Expected event ordering comes from the selected protocol.

Both H and M have release-to-first-execution and release-to-completion metrics.
Each release marker is immediately before its event-release API call; the
controller prevents workers running until the release group is complete and it
blocks. Thus release API and scheduling costs are included. First execution is
the first instrumented point after the event wait returns, not a scheduler hook.

`h_operation` and `l_operation` surround the complete caller transaction.
`m_execution` covers M's workload. `l_resource_work_interval` starts after
acquisition/acceptance, before the controller handshake, and ends after the
payload/shared-word update but before unlock/reply. It includes that setup and
interference; it is not the entire mutex holding interval. `transaction_batch`
is a whole-transaction measurement including harness checks and loop overhead,
not an isolated mutex or channel primitive cost.

## E2: bounded asynchronous transfer

Priorities: controller 1, producer 8, receiver 10, M 5; buffer ceiling either
disabled or 3. Pool capacities are 1 and 4. Four separately initialised pools
are reused only after complete reclamation.

The producer fills the pool while the receiver is parked on an event, then
blocks allocating one more buffer. The controller checks the actual allocator
wait state and verifies that ownership already transferred at **send**, while
the buffers are queued. It then releases receiver and M from a common marker.

- **Handoff:** the receiver frees a buffer directly to the waiting producer,
  which sends one additional message. The receiver processes N+1 messages using
  N physical buffers. Capacity one therefore processes two messages.
- **Timeout:** the receiver remains parked until the extra allocation's 50 ms
  timeout. The producer's contribution must disappear while the receiver's
  custody ceiling remains. The controller then releases receiver/M to drain N.

| Metric | Boundaries |
| --- | --- |
| `work_cycles` | Controller release marker to immediately before the final free |
| `drain_cycles` | Same marker to immediately after the final free returns |
| `allocation_wait_cycles` | Extra allocation call entry to return, including setup/coordination |
| `receiver_dispatch_cycles` | Release marker to first instrumented receiver execution |
| `medium_dispatch_cycles` | Release marker to first instrumented M execution |
| `medium_completion_cycles` | Release marker to M workload completion |
| `medium_execution_interval_cycles` | M workload start to completion |

The last free can remove the ceiling and admit M before the API returns. Keeping
both boundaries exposes that displacement. Checks require M to complete before
the work boundary without a ceiling, after it with a ceiling, and before the
drain boundary in both cases. They also check buffer identity on direct handoff,
payload order, priority withdrawal, ownership and complete pool reclamation.
The controller waits for all relevant completion events, avoiding premature
controller wakeups within the drain interval.

## E3: composition, withdrawal and maintenance cost

The composition case uses G at nominal 8, holding a ceiling-3 buffer, then
waiting on a PIP mutex owned by O at nominal 10. O has no message endpoint.
Both successful handoff and timeout must produce:

1. While blocked: G=3 and O=3.
2. After that dependency ends: G=3 and O=10.
3. After G frees its buffer: G=8 and O=10.

Two further tests combine a ceiling-5 mutex and ceiling-3 buffer in one task.
Releasing the mutex first must give `8→5→3→3→8`; releasing the buffer first must
give `8→5→3→5→8`. These are checked properties, not measured performance claims.

The cost matrix has 0/1/4 **uncontended PIP mutexes with no waiters** and
0/1/4/8 retained ceiling-3 messages, using a nine-buffer pool. All other workers
are parked; only the low-priority awake task remains runnable. Each matrix cell
measures separately:

- 256 `kTaskUpdateEffectivePrio()` calls with unchanged contributors and unchanged
  effective priority (`stable_recalc_batch_cycles`).
- 256 allocation/free **pairs**, using the spare buffer. With N=0 each pair
  changes priority 8→3→8; with N>0 priority stays at 3. The metric name identifies
  the changing or stable path.

`operations=256` means 256 recalculations or 256 complete allocation/free pairs,
respectively. The summary divides the raw batch total by that number. It does
not divide a pair by two, and it does not subtract loop/status-accumulation costs.
Checks and logging are outside cost batches. The composition wait metric
`mutex_wait_with_coordination_cycles` includes coordination and intentional
waiting; it is not isolated propagation overhead.

The matrix characterises one running-task update path and an allocation/free
path. It does not establish WCET or measure all READY/wait-queue repair paths.
The former private harness's exact mutex configuration has not been assumed;
the PIP/no-waiter configuration is explicit in this suite's output.

## Validation and references

The host parser rejects failed/incomplete captures, repeated boots, duplicates,
missing samples, missing cases/metrics, and incorrect timing metadata. Its tests
use visibly synthetic log fixtures only, never experiment results:

```sh
python3 -m unittest discover -s benchmarks/resource_sharing -p 'test_*.py'
```

Compilation and code review do not constitute execution on a physical board.
Record hardware results only after successful capture from the NUCLEO-F103RB.
Retain all samples, build/flash logs and metadata with any paper figures.

Primary hardware documentation:

- [Arm Cortex-M3 Technical Reference Manual, DWT and debug registers](https://www.keil.com/dd/docs/datashts/arm/cortex_m3/r2p0/ddi0337g_cortex_m3_r2p0_trm.pdf)
- [ST UM1724, Nucleo-64 clock and virtual COM configuration](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf)
- [STM32F103RB product documentation](https://www.st.com/en/microcontrollers-microprocessors/stm32f103rb.html)
