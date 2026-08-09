# Changelog

All notable changes to this project will be documented in this file.

## [1.4.5] - 2026-08-08

### Added
- **UDP Status Command**: Get real-time status of processes via UDP
  - New `s<app>` UDP command (e.g., `sBot`); an empty or `*` name returns **all apps**
  - JSON response with: app, status, circuit state, **pid, cpu, mem_kb**, cooldown remaining, failure count, ack requirement
  - Status values: `running`, `stopped`, `crashed`, `cooldown`, `circuit_open`
  - Circuit states: `closed`, `open`, `half_open`, `disabled` (if not configured)
  - `ack_required` field indicates if manual acknowledgment is needed before resuming
- **Status client** (`-s [app]`, `-j` for raw JSON): query a running watchdog and print an
  aligned table (or JSON), instead of hand-crafting UDP packets.
- **Resume over UDP** (`k<app>`): clear cooldown / circuit-breaker state and restart — the
  UDP equivalent of the `resume<app>` file command.
- **Tests**: lean GTest unit/contract suite (parsing, backoff, status-JSON contract) and
  `-t` integration checks (`config_parse`, `status_json`, `heartbeat_timeout`) run via
  `-t all`; both wired into CI.

### Changed
- Enabled control commands (`start<app>`, `stop<app>`, `restart<app>`, `s<app>`, `k<app>`) via UDP
- Server module now tracks sender address for UDP responses
- `-t` now returns a pass/fail exit status (`-t all` gates CI); signal handlers log the
  shutdown cause from the main loop, not from signal context
- Updated README.md (quick-start, status client, testing model, UDP command reference)

### Fixed
- **Progress/stagnation detection was silently disabled**: `config_parse_file()`
  unconditionally reset `progress_timeout` to 0 after parsing, discarding the value read
  from the INI. Because the whole feature is gated on `progress_timeout > 0`, stagnation
  detection never ran for any app despite being documented and shipped default-on. Removed
  the stray reset (the initial `memset` still defaults omitting apps to 0). Added the
  `progress_timeout_config` regression test.
- **Silent permanent outage on UDP-poll failure**: the main loop exited with the default
  `EXIT_NORMALLY`, which the supervisor script treats as a clean stop (no respawn). Now
  exits with `EXIT_RESTART` so the watchdog is restarted. A signal-interrupted poll
  (`EINTR`) is treated as "no data", not a failure.
- **Remote out-of-bounds read in the UDP command parser**: `parse_number` had an unbounded
  scan (ignored length / NUL), signed-`char` `isdigit` UB, and a `char` counter that could
  index before the buffer. Now bounded and `unsigned char`-safe; malformed-datagram tests added.
- **Undefined behaviour in restart backoff**: `base_delay * (1 << (retry_count - 1))` could
  shift past the int width and overflow negative past the 1-hour cap (restart storm) for a
  large `max_retries`. Inputs are now clamped.
- **Divide-by-zero in first-heartbeat stats** guarded (potential `SIGFPE`).
- **Async-signal-unsafe signal handlers**: handlers called `LOGN`/`exit()` (mutex + stdio),
  which could self-deadlock at shutdown. Reduced to flag-setting via `sigaction`; the cause
  is logged from the main loop and the forced-exit path uses `_exit`.
- **Uninitialised reads** on failed `stat`, `sysinfo`, and `clock_gettime` return paths.
- **Potential crash from `localtime`** returning `NULL` (bad `time_t`); replaced with
  `localtime_r` + NULL guards in the timestamp, stats and daily-reboot paths.
- **Config handler rejections were silently accepted**: `config_parse_file` only treated a
  negative `ini_parse` result as failure, but a handler rejecting a bad value returns the
  (positive) line number — so invalid app names, out-of-range integers and duplicate
  sections booted anyway. Any non-zero result is now a failure.
- **Integer overflow in `parse_number`**: a long digit run in a UDP datagram overflowed the
  `int` accumulator (signed-overflow UB); it now clamps at `INT32_MAX` (rejected as an
  invalid pid downstream).
- **Two unchecked `clock_gettime` calls** in the restart/backoff path (uninitialised
  `ts.tv_sec` on failure) now use the checked `get_monotonic_time()` wrapper.
- **`sysconf(_SC_CLK_TCK)` used as a divisor** without checking for `<= 0` (garbage CPU% or
  `SIGFPE`); now guarded.
- **Socket fd leak** in `udp_start` on `setsockopt`/`bind` failure (missing `close`).
- **`circuit_breaker_window * 2`** could overflow `int`; widened to `long`.
- **Config validation**: reject an app section with no `cmd`, a duplicate `[app:X]`, and a
  too-long `cmd` (now checked before it is stored truncated).

### Removed
- Unused, buggy helpers `substring` and `find_replace_text`.

### Build
- CI now cross-builds and publishes Linux binaries for `x86_64`, `aarch64` and `armv7`.

## [1.4.4] - 2026-08-08

### Added
- **Circuit-Breaker (Anti-Flap)**: Prevents cascading failures by opening circuit after repeated failures
  - New `circuit_breaker_threshold` config (0 = disabled, N = failures to open)
  - New `circuit_breaker_window` config (default: 300s/5min for failure counting)
  - Circuit states: CLOSED → OPEN → HALF_OPEN (after 2x window)
  - Automatic retry after circuit opens (one trial)
  - Full exponential backoff integration
  - Clear log messages for state transitions

- **Cooldown (Anti-Flap)**: Prevents rapid restart loops with waiting period
  - New `cooldown_period` config (0 = disabled, N = seconds to wait)
  - New `cooldown_manual_ack_threshold` config (0 = auto-resume, N = require ack after N cooldowns)
  - Manual acknowledgment via `resume<app>` file command
  - Status logging during cooldown period
  - Automatic resume after cooldown if threshold not reached

- **File Command**: Manual acknowledgment for cooldown
  - `resume<app>` - Resume a cooled-down app after manual review

### Changed
- Updated help section in binary to document new config options
- Updated README.md with concise circuit-breaker and cooldown documentation
- Added FlapTest app to config.ini as example

## [1.4.3] - 2026-07-30

### Added
- **Progress Signal (Stagnation Detection)**: Optional feature to detect processes that are alive but stuck/making no progress
  - New `progress_timeout` config field (0 = disabled, >0 = seconds between progress signals)
  - Send `p<pid>+` when work completes (append `+` to heartbeat)
  - Process restarted if no progress signal within `progress_timeout` seconds
  - Full backward compatibility: existing processes work unchanged
  - Auto-adjustment: `progress_timeout` increased if <= `heartbeat_interval`
  - Validation: warnings for unreasonable values (>7 days)
  - Debug logging for progress signal reception
  - Unit tests for heartbeat parsing with/without `+` suffix
  - Comprehensive documentation with examples in 7 programming languages

### Changed
- Centralized `get_monotonic_time()` function in `utils.c`
- Updated test child script to support progress signal intervals: `python test_child.py 1 crash 45`
- Enhanced configuration validation with helpful warnings
- Added best practices documentation for progress timeout values

## [1.4.2] - 2026-07-27

### Changed
- Restart backoff logic now uses `CLOCK_MONOTONIC` for elapsed time calculations (was using wall-clock `time(NULL)`).

## [1.4.1] - 2026-05-26

### Added
- Restart policy with exponential backoff feature.
- Process start time verification to prevent PID reuse issues

## [1.4.0] - 2025-12-27

### Added
- Average heartbeat count tracking before crashes/resets.
- Child process isolation using `setsid()` to prevent signal propagation.
- Exponential backoff with configurable `max_retries` and `base_delay`.
- `--version` flag support (`--version` and `-v` both work).

### Changed
- Heartbeat timing now uses `CLOCK_MONOTONIC` to prevent issues with NTP adjustments and system clock changes.
- Restart backoff logic now uses `CLOCK_MONOTONIC` for elapsed time calculations.
- Signal handler variables now use `sig_atomic_t` for safe signal handling.
- CPU statistics use `float` instead of `double` to reduce memory footprint.
- Separated CPU and memory monitoring into independent functions.

### Fixed
- Static buffer in `printDate()` replaced with caller-provided buffer.
- Buffer overflow in stats filename generation (increased buffer size).
- Child processes now close inherited file descriptors before exec.

## [1.3.0] - 2025-08-24

### Added
- Flexible periodic reboot feature. Supports daily time (`HH:MM`) and intervals in hours (`h`), days (`d`), weeks (`w`), and months (`m`).

## [1.2.0] - 2025-08-23

### Added
- Monitor and log CPU and memory usage for each application.

## [1.1.0] - 2025-08-17

### Added
- CMake support for building the project.

### Changed
- Refactored code: inlined wrappers, tidied up, and reformatted.
- Modularized code: moved command parsing to `cmd.c`, heartbeat functions to `heartbeat.c`, process control functions to `process.c`, and INI parsing to `config.c`.

## [1.1.0] - 2025-06-30

### Changed
- Improved INI format for arrays of processes.

## [1.0.0] - 2025-04-26

### Changed
- Updated GitHub Actions for building and testing.
- Improved makefile.
- Enhanced application management: renamed `nWdtApps` to `n_apps`, improved start/restart/terminate functionality.
- Improved config parsing safety.
- Added support for non-heartbeat processes.
- Updated `test_child` to support non-heartbeat processes.
- Print unknown commands as hex with safe bounds and printable fallback.

### Fixed
- Quoting in `run.sh`.
- Reduced test time to 300 seconds.
- Set signal to USR1 for normal exit.
- Log file printing.

## [0.1.0] - 2024-08-31

### Added
- Initial project setup, CI creation, and basic makefile.

### Changed
- Replaced "ping" with "heartbeat" in code and INI configuration.
- Updated README.md and Makefile.
