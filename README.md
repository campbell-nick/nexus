# nexus

A small, fast spinning 3D terminal fetch tool. `nexus` lifts an ASCII/Unicode
logo into a height map, extrudes it into a point cloud, and renders it as a
rotating relief with Blinn-Phong shading, with system information shown beside
it.

It is a from-scratch, modular C program inspired by the single-file
[`fetch`](https://github.com/areofyl/fetch) tool. Everything is kept minimal:
info comes straight from `/proc`, `/sys` and `/etc`, so startup stays quick.
The only build dependency is `libm`. If `fastfetch` is installed at runtime its
logo library is used for authentic distro logos; otherwise a small embedded set
is drawn. Linux is the primary target, with best-effort macOS support behind
`#ifdef __APPLE__`.

## Build & run

```
make
./nexus
```

The build is plain C11 and works with both **glibc** and **musl**, and with
**gcc** or **clang**. It only needs `libm` and the kernel headers; there is no
configure step. A `statx()` feature probe enables the `age` field when the libc
provides it (glibc ≥ 2.28, musl ≥ 1.2.4) and omits it otherwise, so the same
source builds unchanged on older systems.

```
make musl          # link against musl (needs musl-gcc)
make STATIC=1      # statically linked binary
make static        # same thing

# Cross-compiling for another architecture, e.g. 64-bit ARM:
make CC=aarch64-linux-musl-gcc STATIC=1
# ...or, with no separate toolchain, using zig's bundled musl:
make CC="zig cc -target aarch64-linux-musl" STATIC=1
```

It runs on **x86_64, aarch64/arm64, armv7 and other Linux architectures**: CPU
detection reads `/proc/cpuinfo` (`model name`, `Model` or `Hardware`) and falls
back to `/proc/device-tree/model` for boards that expose nothing else, and the
GPU field recognises common SoC drivers (Broadcom VideoCore, Qualcomm Adreno,
ARM Mali, Vivante) alongside Intel, AMD and NVIDIA.

Press any key to stop. Function keys (F1–F12), arrow keys and other
escape-sequence keys are ignored, so only ordinary typing keys dismiss it.
Ctrl-C works too.

```
sudo make install            # installs to /usr/local/bin/nexus
PREFIX=~/.local make install # ...or into your home
```

## Options

| Flag | Description |
|------|-------------|
| `-l`, `--logo <name\|path>` | Show a distro logo by name (e.g. `arch`), or load a logo file |
| `--rotate-x` | Lock rotation to the X axis only |
| `--rotate-y` | Lock rotation to the Y axis only |
| `-s`, `--speed <float>` | Speed multiplier (default `1.0`) |
| `--size <float>` | Scale the logo (e.g. `2.0` for double size) |
| `--depth <float>` | Scale the 3D depth (default `1.0`) |
| `--height <n>` | Override the render height in rows |
| `--no-info` | Just the logo, no system info |
| `--no-color` | Disable logo coloring |
| `--frames <n>` | Stop after `n` frames (default: run until a keypress) |
| `--shading-chars <str>` | Custom shading ramp (UTF-8 aware) |
| `-h`, `--help` | Show help |
| `-V`, `--version` | Show version |

## Logos

A logo is chosen automatically, in this order:

1. `--logo <path>` — an explicit file
2. `~/.config/nexus/logo.txt`
3. **fastfetch** — if installed, its built-in library supplies a full colour
   logo for the detected distribution
4. the **embedded set** — plain-text logos for ~50 distributions (arch, gentoo,
   ubuntu, debian, fedora, nixos, void, alpine, opensuse, suse, manjaro, mint,
   pop, endeavouros, kali, rocky, alma, centos, rhel, oracle, amazon, artix,
   garuda, cachyos, arco, archcraft, blackarch, parrot, nobara, devuan,
   raspbian, zorin, elementary, deepin, solus, neon, mx, antix, mageia,
   openmandriva, trisquel, pureos, linuxlite, slackware, freebsd, openbsd,
   netbsd, funtoo, ...)
5. a plain Tux, then a blocky "N"

Name a distribution to show its logo:

```
./nexus --logo arch
```

The name is looked up in fastfetch first, then the embedded set, and it also
selects the tint. Anything with a path separator or extension is treated as a
file instead:

```
./nexus --logo ~/art/logo.txt
```

A logo file is plain text. An optional first line `# distro: <name>` overrides
the detected distribution, which selects both the logo and its colour:

```
# distro: gentoo
         -/oyddmdhs+:.
     -odNMMMMMMMMNNmhy+-`
...
```

The distribution is read from `/etc/os-release` (`ID`, then `ID_LIKE`). Logos
borrowed from fastfetch keep their own ANSI colours; plain logos (files or the
embedded set) are tinted with the distribution's canonical colour. Unknown
distributions keep the default magenta/white scheme.

## Shading

Brightness is mapped onto a ramp, one character per cell. The built-in ramp is
`.,-~:;=!*#$@`; override it on the command line:

```
./nexus --shading-chars '#*.'
```

or in the config file (see below):

```
shading_chars = o
```

## Configuration

On first run nexus writes `~/.config/nexus/nexus.conf` (honouring
`XDG_CONFIG_HOME`) with a commented template. It is a list of `key = value`
lines; blank lines and lines starting with `#` are ignored. Every command-line
option has a matching key, and **flags always win over the file**:

| Key             | Values                          | Equivalent flag       |
| --------------- | ------------------------------- | --------------------- |
| `logo`          | file path or distro/logo name   | `--logo`              |
| `shading_chars` | any ramp; `default` for built-in| `--shading-chars`     |
| `rotation`      | `x`, `y` or `both`              | `--rotate-x`/`--rotate-y` |
| `speed`         | number, e.g. `1.0`              | `--speed`             |
| `size`          | number, e.g. `1.0`              | `--size`              |
| `depth`         | number, e.g. `1.0`              | `--depth`             |
| `height`        | rows; `0` = auto                | `--height`            |
| `frames`        | count; `0` = until a keypress   | `--frames`            |
| `info`          | `true`/`false`                  | `--no-info` (inverse) |
| `color`         | `true`/`false`                  | `--no-color` (inverse)|
| `palette`       | `true`/`false`                  | `--no-palette` (inverse)|

Booleans also accept `yes`/`no`, `on`/`off` and `1`/`0`. A `default` value (or
a value equal to the built-in default) leaves the built-in behaviour in place.
An unknown value prints a warning and keeps the built-in default.

## Info fields

The default (and only) field set is deliberately small and cheap to gather:

```
os, host, kernel, uptime, age, packages, shell, cpu, gpu, memory, swap, disk
```

A `user@host` title with a separator line is always shown above them. `age` is
how long ago the distribution was installed, taken from the root filesystem's
birth time; it is omitted when the filesystem does not report one. Package
counts come from emerge, pacman, dpkg, rpm, xbps, apk, eopkg or nix, whichever
is present.

Below the fields, nexus draws the 16 standard terminal colours as two rows of
eight blocks. Hide it with `--no-palette` (or `palette = false`); it is also
hidden by `--no-info` and `--no-color`.

## Layout

```
nexus/
  Makefile
  src/
    nexus.h    shared declarations
    util.c     text / terminal / parsing helpers
    config.c   config file reader and first-run template
    logo.c     logo loading, decoding and distro tinting
    distros.c  embedded plain-text distro logos (generated)
    info.c     native system-information gathers
    render.c   height map, projection, shading and the animation loop
    main.c     CLI parsing and orchestration
```

## Changelog

See [CHANGELOG.md](CHANGELOG.md).

## License

MIT. See [LICENSE](LICENSE).
