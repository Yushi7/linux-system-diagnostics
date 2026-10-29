# Linux System Diagnostics and Driver Toolkit

A C++20 command-line tool that reads Linux system statistics from `/proc`. The current MVP reports aggregate CPU usage, memory totals, and a PID-sorted process list with resident-memory usage where available.

## Current Status

The MVP implementation, unit tests, live `/proc` integration test, and benchmark are present. No kernel module, CUDA component, or userspace driver client is implemented; those are explicitly outside the MVP.

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
sudo apt install -y build-essential cmake ninja-build git gdb clang-format libgtest-dev
```

Check the toolchain with `g++ --version`, `cmake --version`, `ninja --version`, and `git --version`.

## Build and Test

This workspace is currently stored on the Windows filesystem. In WSL, keep CMake's generated files on the Linux filesystem; generating a build tree under `/mnt/c` failed with `Operation not permitted` during compiler checks.

For this checkout, run in the Ubuntu terminal:

```bash
cd "/mnt/c/Users/BIT/Linux Project"
cmake -S . -B "$HOME/build/linux-system-diagnostics" -G Ninja
cmake --build "$HOME/build/linux-system-diagnostics"
ctest --test-dir "$HOME/build/linux-system-diagnostics" --output-on-failure
```

For a clone stored in the Linux filesystem, use its project directory as the source and a build directory such as `build`:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

## Usage

Run all diagnostic sections (CPU sampling takes 250 ms by default):

```bash
./build/linux-sysdiag
```

Select sections and configure the sampling interval or process display limit:

```bash
./build/linux-sysdiag --cpu --memory --sample-interval-ms 500
./build/linux-sysdiag --processes --max-processes 10
./build/linux-sysdiag --help
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
  Used:      474.4
  Available: 3092.8
  Free:      2979.2

Processes (31):
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

## Tests and Measurements

Verified on 2026-10-29 with Ubuntu 24.04.5 LTS, WSL2 Linux kernel 6.6.87.2, and GCC 13.3.0:

- CTest: **15 passed, 0 failed**, including CLI, parser, CPU calculation, memory fallback, process fixtures, and live `/proc` integration.
- Benchmark: 20 iterations, 250 ms configured sample interval, 32 processes observed.

| Operation                            |       Mean |     Median |        p95 |
| ------------------------------------ | ---------: | ---------: | ---------: |
| CPU stat reads and usage calculation |  108.82 us |   91.72 us |  186.21 us |
| Memory info read                     |   81.02 us |   75.98 us |  114.35 us |
| Process enumeration                  | 2318.26 us | 2134.96 us | 3130.96 us |
| Total monitoring work                | 2509.05 us | 2305.11 us | 3386.51 us |

Estimated monitoring work was **1.00%** of the configured 250 ms interval. This is the mean measured work time divided by the interval; the benchmark excludes the intentional sampling sleep. Results are one WSL run, not a performance guarantee. Process enumeration dominated this sample.

`perf` hardware-counter profiling was unavailable because it is not installed in the verified WSL environment. These benchmark timings are not a substitute for hardware-counter profiling.

## Limitations

- CPU usage is an aggregate sample and can round to `0.0%` at the CLI's display precision when activity is very low.
- The memory fallback is an estimate; `MemAvailable` is preferred whenever the kernel provides it.
- `/proc` is inherently racy: processes can disappear or become inaccessible between enumeration and reads.
- The process limit only caps output after enumeration, so it does not reduce collection cost.
- WSL reports the Linux environment's view of resources, not a complete Windows host inventory.
- No kernel module is built or loaded. Kernel development requires a disposable VM and must not be experimented with on a primary machine.

## GitHub Preparation

The source workspace has a local Git repository and an initial project checkpoint, but no GitHub remote is configured. Create an empty repository on GitHub, then publish the existing local branch:

```bash
git status --short
git commit -m "Add Linux system diagnostics MVP"
git remote add origin https://github.com/YOUR_ACCOUNT/YOUR_REPOSITORY.git
git push -u origin main
```

Replace the remote URL with your repository. Authenticate with GitHub's supported credential flow; do not put tokens or passwords in the remote URL. The existing `.vscode/settings.json` is intentionally excluded from the project checkpoint. For later milestones, stage the project files, review `git status`, then create a commit with a concise message.
