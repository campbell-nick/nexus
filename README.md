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

Press any key to stop (the keypress passes through to the shell, so it works as
a startup fetch). Ctrl-C works too.

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

Brightness is mapped onto an ASCII ramp, one character per cell. The default
ramp is `.,-~:;=!*#$@`; override it with `--shading-chars`, for example:

```
./nexus --shading-chars '#*.'
```

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

## Layout

```
nexus/
  Makefile
  src/
    nexus.h    shared declarations
    util.c     text / terminal / parsing helpers
    logo.c     logo loading, decoding and distro tinting
    distros.c  embedded plain-text distro logos (generated)
    info.c     native system-information gathers
    render.c   height map, projection, shading and the animation loop
    main.c     CLI parsing and orchestration
```

## License

MIT. See [LICENSE](LICENSE).
