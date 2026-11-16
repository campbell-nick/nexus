# Dependencies

`nexus` is deliberately tiny. It links only against `libm` and reads `/proc`,
`/sys` and `/etc` directly, so it needs almost nothing to build or run — no
`configure`, no `pkg-config`, no third-party libraries.

## Build

| Needed | Why |
| ------ | --- |
| A C compiler — **gcc** or **clang** | compiles the sources |
| **make** | runs the `Makefile` |
| The C library's development headers (**glibc** or **musl**) | the POSIX/system headers `nexus` includes (`stdio.h`, `sys/stat.h`, `termios.h`, ...) |
| **libm** | the math functions used for the 3D projection and shading |

`libm` ships with the libc development package on both glibc and musl, so in
practice a compiler plus `make` is all you install. Kernel headers are **not**
required: `nexus` uses only standard libc headers, no `<linux/...>` headers.

## Runtime

| Needed | Why |
| ------ | --- |
| The C library (glibc or musl) | a **static** build needs nothing at all |
| **fastfetch** *(optional)* | its logo library supplies full-colour distro logos; without it `nexus` draws its own embedded logos |
| A UTF-8 terminal | for the Unicode logos and glyphs |

Two fields depend on tools that are already present on most systems, and are
used only when asked for:

- `packages` counts packages with whichever manager the system has — emerge,
  pacman, dpkg, rpm, xbps, apk, eopkg or nix.
- `nix` is opt-in and needs a Nix installation to walk the store.

## Installing

| Distribution | Build | Optional (`fastfetch`) |
| ------------ | ----- | ---------------------- |
| Gentoo | `emerge sys-devel/gcc sys-devel/make` | `emerge app-misc/fastfetch` |
| Debian / Ubuntu | `sudo apt install build-essential` | `sudo apt install fastfetch` |
| Fedora | `sudo dnf install gcc make glibc-devel` | `sudo dnf install fastfetch` |
| Arch | `sudo pacman -S base-devel` | `sudo pacman -S fastfetch` |
| Alpine | `sudo apk add build-base` | `sudo apk add fastfetch` |
| openSUSE | `sudo zypper install gcc make glibc-devel` | `sudo zypper install fastfetch` |
| Void | `sudo xbps-install base-devel` | `sudo xbps-install fastfetch` |
| Nix | `nix develop` (see [building.md](building.md)) | — |

`build-essential` (Debian/Ubuntu), `base-devel` (Arch/Void) and `build-base`
(Alpine) are meta-packages that pull in the compiler, `make` and the libc
development headers. On musl distributions such as Alpine that includes
`musl-dev`; on glibc distributions it includes `glibc-devel`.

Then just build:

```
make
./nexus
```

## Portable binaries

A statically linked build (`make STATIC=1`) has **no runtime library
dependencies** — a single file that runs on any Linux of the same architecture.
Only a compiler and `make` are needed to produce it. See
[musl.md](musl.md) for the static and cross-compiling builds.
