# Linux System Diagnostics Toolkit

A C++20 Linux userspace command-line tool that reads system statistics from `/proc`. It reports aggregate CPU usage, memory totals, and a PID-sorted process list with resident-memory usage where available.

## Current Status

This release is complete within its declared Phase 1–7 scope: the userspace diagnostic tool, tests, live `/proc` integration test, benchmark, and documentation. No Phase 8 kernel-level work is included.

## Requirements

- Linux with procfs mounted at `/proc` (Ubuntu 24.04 LTS under WSL2 is the verified environment)
- GCC or Clang with C++20 support
- CMake 3.20 or newer
- Ninja
- GoogleTest development package for the test target

On Windows, install WSL from **PowerShell** if needed, then launch Ubuntu and create its Linux user:

```powershell
wsl --install -d Ubuntu-24.04
```

In the Ubuntu terminal, install the build and test dependencies:

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build git libgtest-dev
```

Check the toolchain with `g++ --version`, `cmake --version`, `ninja --version`, and `git --version`.

## Build and Test

This workspace is currently stored on the Windows filesystem. In WSL, keep CMake's generated files on the Linux filesystem; generating a build tree under `/mnt/c` failed with `Operation not permitted` during compiler checks.

For this checkout, run in the Ubuntu terminal (replace the `cd` path if your workspace is elsewhere):

```bash
cd "/mnt/c/Users/BIT/Linux Project"
BUILD_DIR="$HOME/build/linux-system-diagnostics"
cmake -S . -B "$BUILD_DIR" -G Ninja
cmake --build "$BUILD_DIR"
ctest --test-dir "$BUILD_DIR" --output-on-failure
```

For a clone stored in the Linux filesystem, replace this example path with the actual clone directory, then run the same build commands:

```bash
cd /path/to/linux-system-diagnostics
BUILD_DIR="$HOME/build/linux-system-diagnostics"
cmake -S . -B "$BUILD_DIR" -G Ninja
cmake --build "$BUILD_DIR"
ctest --test-dir "$BUILD_DIR" --output-on-failure
```

## Usage

Run all diagnostic sections (CPU sampling takes 250 ms by default):

```bash
"$HOME/build/linux-system-diagnostics/linux-sysdiag"
```

Select sections and configure the sampling interval or process display limit:

```bash
"$HOME/build/linux-system-diagnostics/linux-sysdiag" --cpu --memory --sample-interval-ms 500
"$HOME/build/linux-system-diagnostics/linux-sysdiag" --processes --max-processes 10
"$HOME/build/linux-system-diagnostics/linux-sysdiag" --help
```

`--proc-root PATH` points the readers at an alternate procfs-style tree, which is useful for controlled fixtures. Section flags can be combined; if none is given, all sections are selected. `--max-processes` limits displayed rows, not the enumeration work.

Exit codes are `0` when selected diagnostics succeed, `1` when a diagnostic section fails, and `2` for invalid command-line arguments. A section error is printed to standard error while other selected sections continue.

### Terminal Demonstration

Captured from Ubuntu 24.04 on WSL2 using `--sample-interval-ms 100 --max-processes 3`:

```text
CPU usage (100 ms sample): 0.0%
CPU time breakdown: user 0.0%, system 0.0%, idle 100.0%, iowait 0.0%

Memory (MiB):
  Total:     3567.2
  Used:      509.0
  Available: 3058.2
  Free:      2948.3

Processes (33):
Showing first 3 processes.
PID     NAME                    RSS (MiB)
1       systemd                       12.2
2       init-systemd(Ub                1.6
6       init                           1.9
```

This text demonstration is a captured run, not a synthetic screenshot. Runtime values vary between systems and runs.

## Architecture

```text
include/sysdiag/   Public data structures and module interfaces
src/               CLI, CPU, memory, process readers, and executable entry point
tests/             GoogleTest unit tests and live /proc integration test
benchmarks/        Standalone timing and monitoring-overhead benchmark
CMakeLists.txt     Library, executable, benchmark, and test targets
```

- **CPU:** Reads aggregate jiffy counters from `/proc/stat`; usage is calculated from two samples. User, system, idle, and I/O-wait percentages are shown.
- **Memory:** Parses `/proc/meminfo`. Used memory is `MemTotal - MemAvailable`; free memory is reported separately. If `MemAvailable` is absent, the reader reports a documented estimate.
- **Processes:** Enumerates numeric `/proc` directories, reads names from `comm`, and reads RSS from `status` (`VmRSS`). Processes that disappear or cannot provide a name are skipped; RSS may be shown as unavailable.
- **Errors:** Readers throw descriptive exceptions for section-level failures. The CLI reports those failures independently.

### Benchmark

Run the standalone benchmark with:

```bash
"$HOME/build/linux-system-diagnostics/linux-sysdiag-benchmark" --iterations 30 --sample-interval-ms 250
```

It reports mean, median, and p95 reader times and estimates work as a percentage of the configured sample interval. The estimate excludes the intentional sampling sleep; it is not a measurement of total wall-clock sampling latency.

## Tests and Measurements

Verified on 2026-10-29 with Ubuntu 24.04.5 LTS, WSL2 Linux kernel 6.6.87.2, and GCC 13.3.0:

- CTest: **16 passed, 0 failed**, including CLI, parser, CPU calculation, memory fallback, process fixtures, and live `/proc` integration.
- Benchmark: 30 iterations, 250 ms configured sample interval, 36 processes observed.

| Operation                            |       Mean |     Median |        p95 |
| ------------------------------------ | ---------: | ---------: | ---------: |
| CPU stat reads and usage calculation |  121.65 us |   97.04 us |  283.65 us |
| Memory info read                     |   85.43 us |   72.97 us |  161.87 us |
| Process enumeration                  | 2690.44 us | 2455.58 us | 4144.64 us |
| Total monitoring work                | 2898.31 us | 2674.52 us | 4443.81 us |

Estimated monitoring work was **1.16%** of the configured 250 ms interval. These measurements are from one WSL run, not performance guarantees; process enumeration was the largest component in this sample. `/usr/bin/time -v` reported 0.09 seconds wall time and 4,096 KiB maximum RSS for the same 30-iteration run.

`perf` hardware-counter profiling was unavailable because it is not installed in the verified WSL environment. These benchmark timings are not a substitute for hardware-counter profiling.

## Limitations

- CPU usage is an aggregate sample and can round to `0.0%` at the CLI's display precision when activity is very low.
- The memory fallback is an estimate; `MemAvailable` is preferred whenever the kernel provides it.
- `/proc` is inherently racy: processes can disappear or become inaccessible between enumeration and reads.
- The process limit only caps output after enumeration, so it does not reduce collection cost.
- WSL reports the Linux environment's view of resources, not a complete Windows host inventory.

## Out of Scope / Future Work

Kernel modules, device drivers, kernel/userspace communication, and CUDA monitoring are not implemented in this release. They are excluded from the Phase 1–7 completion claim and are not required to build, use, test, or release this userspace toolkit.

## GitHub Preparation

The source workspace has an initial local checkpoint, but final audit fixes may be uncommitted and no GitHub remote is configured. Review the working tree and tests, then manually create a GitHub repository and publish when ready:

```bash
git status --short
git add .gitignore CMakeLists.txt README.md benchmarks include src tests
git diff --cached --check
git commit -m "Finalize Linux userspace diagnostics release"
git remote add origin https://github.com/YOUR_ACCOUNT/YOUR_REPOSITORY.git
git push -u origin main
```

Replace the remote URL with your repository. Authenticate with GitHub's supported credential flow; do not put tokens or passwords in the remote URL. The existing `.vscode/settings.json` is intentionally excluded. These commands are a manual release checklist; no remote or push has been created by this project workflow.
