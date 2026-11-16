# Building and installing

```
make
./nexus
```

The build is plain C11 and works with both **glibc** and **musl**, and with
**gcc** or **clang**. It only needs `libm` and the C library's headers; there is
no configure step. See [dependencies.md](dependencies.md) for the packages to
install. A `statx()` feature probe enables the `age` field when the libc
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

For the full guide — installing `musl-gcc`, static builds, and cross toolchains
(crossdev, musl.cc, zig) — see [musl.md](musl.md).

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

## Nix

A `flake.nix` is included, so Nix users can build and develop without installing
a toolchain:

```
nix build            # -> ./result/bin/nexus
nix run              # build and run
nix develop          # dev shell with gcc, clang and make
nix build .#musl     # same source linked against musl, statically (Linux)
```
