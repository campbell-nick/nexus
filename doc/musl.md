# Building nexus against musl

`nexus` is plain C11 with no configure step. It needs only `libm` and the C
library's headers, so the same source builds unchanged against **glibc** or
**musl**. Building against musl — and especially statically — produces a single
self-contained binary that runs on any Linux of the same architecture, with no
runtime library dependencies.

This guide covers the native musl build, static builds, and several ways to
cross-compile.

## Quick reference

| Goal | Command |
| ---- | ------- |
| Native musl (same arch) | `make musl` |
| Static binary (any libc) | `make STATIC=1` |
| Static + musl | `make musl STATIC=1` |
| Cross, x86_64 musl | `make CC=x86_64-unknown-linux-musl-gcc STATIC=1` |
| Cross, aarch64 musl | `make CC=aarch64-linux-musl-gcc STATIC=1` |
| Cross with zig (no toolchain) | `make CC="zig cc -target aarch64-linux-musl" STATIC=1` |
| Nix | `nix build .#musl` |

## Prerequisites

- A C compiler (`gcc` or `clang`) and `make` (see
  [dependencies.md](dependencies.md) for the base packages).
- For a **native** musl build: the `musl-gcc` wrapper (see below).
- Nothing else — no `configure`, no autotools, no extra libraries.

## Native musl build

`make musl` is a shortcut for `make CC=musl-gcc`:

```
make musl
```

`musl-gcc` is a small wrapper that drives your system compiler so that it links
against musl instead of glibc. Install it from your distribution:

| Distribution | Package | Provides |
| ------------ | ------- | -------- |
| Gentoo | `sys-libs/musl` | `musl-gcc` |
| Debian / Ubuntu | `musl-tools` | `musl-gcc` |
| Fedora | `musl-gcc` | `musl-gcc` |
| Arch | `musl` | `musl-gcc` |

```
# examples
sudo emerge sys-libs/musl          # Gentoo
sudo apt install musl-tools        # Debian / Ubuntu
sudo dnf install musl-gcc          # Fedora
sudo pacman -S musl                # Arch
```

A native musl build is still **dynamically** linked (against musl's `libc.so`),
so it will not run on a host that only has glibc. For a portable binary, add
`STATIC=1` as described below.

## Static builds

Pass `STATIC=1` (or use the `static` shortcut) to add `-static` to the link:

```
make STATIC=1     # static, glibc
make static       # same thing
make musl STATIC=1  # static, musl  (recommended for distribution)
```

- **Static musl** is the usual choice for a portable binary: one file, no
  runtime dependencies, no libc version to match.
- **Static glibc** also works. glibc generally discourages static linking
  because of NSS, `getaddrinfo` and `dlopen`, but `nexus` uses none of those, so
  there is no practical downside here.

Variables set on the command line are passed down to the `make musl` sub-make,
which is why `make musl STATIC=1` works.

## Cross-compiling

To build for a different architecture you need a toolchain whose target is
`<arch>-linux-musl`. There are several ways to get one.

### Option A — Gentoo crossdev (builds from source)

crossdev generates and installs a full cross toolchain for a target tuple. Your
host tuple must differ from the target; a different libc (musl vs glibc) is
enough, so a musl target is always allowed on a glibc host.

```
sudo emerge --ask sys-devel/crossdev
sudo eselect repository create crossdev
sudo crossdev --target x86_64-unknown-linux-musl
```

This compiles binutils, gcc, the kernel headers and musl from source, so it
takes a while (the aarch64 build especially). When it finishes, the wrappers
land in `/usr/bin` via `gcc-config` — no `PATH` edits needed.

```
make clean
make CC=x86_64-unknown-linux-musl-gcc STATIC=1
```

For 64-bit ARM, run a second target and build with its compiler:

```
sudo crossdev --target aarch64-unknown-linux-musl
make clean
make CC=aarch64-unknown-linux-musl-gcc STATIC=1
```

If emerge complains about keywords for the generated `cross-*` packages, accept
them in `/etc/portage/package.accept_keywords`, e.g.:

```
cross-x86_64-unknown-linux-musl/* ~amd64
```

To remove a crossdev toolchain later:

```
sudo crossdev --clean --target x86_64-unknown-linux-musl
sudo eselect repository remove crossdev
sudo emerge --ask --depclean sys-devel/crossdev
```

### Option B — prebuilt musl.cc toolchains

[musl.cc](https://musl.cc) publishes ready-made cross toolchains as tarballs.
Download, extract, and put the `bin` directory on `PATH`:

```
mkdir -p ~/musl-cross && cd ~/musl-cross
curl -fLO https://musl.cc/x86_64-linux-musl-cross.tgz
curl -fLO https://musl.cc/aarch64-linux-musl-cross.tgz
tar xzf x86_64-linux-musl-cross.tgz
tar xzf aarch64-linux-musl-cross.tgz
export PATH="$HOME/musl-cross/x86_64-linux-musl-cross/bin:$HOME/musl-cross/aarch64-linux-musl-cross/bin:$PATH"
```

Then build with the matching compiler:

```
make clean
make CC=x86_64-linux-musl-gcc STATIC=1     # or aarch64-linux-musl-gcc
```

These toolchains are large (hundreds of MB each) and are self-contained, so
adding their `bin` directory to `PATH` is the only integration step.

### Option C — zig (no separate toolchain)

[zig](https://ziglang.org) ships its own C compiler and musl libc for many
targets, so you can cross-compile without installing anything else:

```
make clean
make CC="zig cc -target aarch64-linux-musl" STATIC=1
```

Swap the `-target` triple for other architectures (`x86_64-linux-musl`,
`arm-linux-musleabihf`, ...).

### Option D — distribution cross packages

Some distributions package cross compilers directly. For example Debian and
Ubuntu ship `gcc-aarch64-linux-gnu` (glibc) and musl cross packages under
`musl-tools`-style names; check your package manager for a `<triple>-gcc` that
targets `-linux-musl`. Where one exists, it works exactly like the wrappers
above: `make CC=<triple>-gcc STATIC=1`.

## Nix

The included flake builds the musl variant directly, with no manual toolchain:

```
nix build .#musl     # -> ./result/bin/nexus, statically linked against musl
```

## Verifying the result

After any cross or static build, confirm what you produced:

```
file nexus
# e.g. "ELF 64-bit LSB executable, x86-64, ... statically linked"

ldd nexus
# "not a dynamic executable"  for a static build

./nexus --version
# nexus <version> "<name>" (<arch>, Linux)
```

`--version` reports the architecture the binary was compiled for, which is a
quick sanity check that the cross build targeted what you expected.

## How nexus adapts to the libc

The only libc-sensitive feature is the `age` field (how long ago the
distribution was installed), which uses `statx()`. The `Makefile` probes for it
at build time and enables the field when the libc provides it (glibc ≥ 2.28,
musl ≥ 1.2.4), and omits it otherwise. Everything else reads `/proc`, `/sys`
and `/etc` directly, so it is identical across libcs.

## Troubleshooting

| Symptom | Cause / fix |
| ------- | ----------- |
| `musl-gcc: not found` | The musl wrapper isn't installed — see the package table above. |
| `crossdev: target and host are the same` | crossdev refuses a target identical to the host tuple. A musl target differs from a glibc host, so this only happens if you pick the host's own tuple. |
| `undefined reference` at link with `-static` | The target's static libc isn't installed. For crossdev, re-run the target so musl is rebuilt; for musl.cc/zig it is bundled. |
| emerge keyword errors on `cross-*` | Accept the package keywords (see Option A). |
| Binary runs locally but not elsewhere | You built dynamically against musl; rebuild with `STATIC=1` for portability. |
