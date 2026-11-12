# Changelog

All notable changes to nexus are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- A `flake.nix`, so Nix users can `nix build`, `nix run`, `nix develop`, and
  build the static musl variant with `nix build .#musl`.

## [1.4.0] - 2026-11-12

### Added

- Portability: the same source now builds against both **musl** and **glibc**
  (and with gcc or clang). New `make musl`, `make static`/`STATIC=1` targets and
  a `statx()` feature probe, so the `age` field works where available and is
  omitted on older libcs.
- Architecture support beyond x86_64: CPU detection reads the `Model`/`Hardware`
  fields and falls back to `/proc/device-tree/model`, and the GPU field maps
  common ARM SoC drivers (Broadcom VideoCore, Qualcomm Adreno, ARM Mali,
  Vivante) in addition to Intel, AMD and NVIDIA.
- A GitHub Actions build matrix covering glibc (gcc and clang), native musl,
  Alpine/musl, and aarch64/armv7 cross builds run under qemu.

## [1.3.0] - 2026-11-12

### Added

- Colour palette: the 16 standard terminal colours are drawn as two rows of
  eight blocks under the info, fastfetch-style. Hide it with `--no-palette` or
  `palette = false`; it is also hidden by `--no-info` and `--no-color`.

## [1.2.0] - 2026-11-12

### Added

- `nexus.conf` now mirrors every command-line option, not just `shading_chars`:
  `logo`, `rotation`, `speed`, `size`, `depth`, `height`, `frames`, `info` and
  `color`. Boolean values accept `true`/`false`, `yes`/`no`, `on`/`off` and
  `1`/`0`. Command-line flags still take precedence, and an unknown value warns
  and keeps the built-in default.

### Changed

- `config_get` writes into a caller-supplied buffer instead of returning a
  shared static one, so several keys can be read safely in a row.
- A config value equal to the built-in default no longer disables the
  automatic depth-boost for flat logos, so the shipped template renders the
  same as having no config at all.

## [1.1.0] - 2026-11-11

### Added

- Stop the animation with a single keypress: any ordinary typing key ends it
  immediately, without waiting for Enter. Function keys (F1–F12), arrow keys
  and other escape-sequence keys are ignored, as are control-key combos;
  Ctrl-C and Ctrl-Z keep their usual behaviour. The terminal is restored on
  every exit path.
- Config file at `~/.config/nexus/nexus.conf` (honouring `XDG_CONFIG_HOME`),
  created with a commented template on first run. It currently supports
  `shading_chars`; the value `default` restores the built-in ramp. Command-line
  flags take precedence over the file.

### Changed

- The shading ramp may now be set in the config file, not only with
  `--shading-chars`.

## [1.0.0] - 2026-11-11

### Added

- Spinning 3D ASCII/Unicode logo rendered as a Blinn-Phong-shaded relief, with
  native system information (os, host, kernel, uptime, age, packages, shell,
  cpu, gpu, memory, swap, disk) read straight from `/proc`, `/sys` and `/etc`.
- Logos from the fastfetch library when installed, otherwise an embedded set of
  ~50 distributions tinted with each distro's canonical colour, plus logo-file
  and Tux fallbacks. `--logo <name|path>` selects one explicitly.
- Package counts for emerge, pacman, dpkg, rpm, xbps, apk, eopkg and nix.

[Unreleased]: https://github.com/campbell-nick/nexus/compare/v1.4.0...HEAD
[1.4.0]: https://github.com/campbell-nick/nexus/compare/v1.3.0...v1.4.0
[1.3.0]: https://github.com/campbell-nick/nexus/compare/v1.2.0...v1.3.0
[1.2.0]: https://github.com/campbell-nick/nexus/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/campbell-nick/nexus/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/campbell-nick/nexus/releases/tag/v1.0.0
