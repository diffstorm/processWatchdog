# Changelog

All notable changes to this project will be documented in this file.

## [1.4.0] - 2025-12-27

### Added
- Average heartbeat count tracking before crashes/resets.
- Child process isolation using `setsid()` to prevent signal propagation.

### Changed
- Heartbeat timing now uses `CLOCK_MONOTONIC` instead of wall-clock time to prevent issues with NTP adjustments and system clock changes.
- Signal handler variables now use `sig_atomic_t` for safe signal handling.
- CPU statistics use `float` instead of `double` to reduce memory footprint.
- Separated CPU and memory monitoring into independent functions.

### Fixed
- Static buffer in `printDate()` replaced with caller-provided buffer.
- Buffer overflow in stats filename generation.
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
