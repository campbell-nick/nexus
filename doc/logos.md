# Logos

A logo is chosen automatically, in this order:

1. `logo = <path>` in the config, or `--logo <path>` — an explicit file
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

The same can be set in the config (`logo = /home/you/art/logo.txt`). A leading
`~` is expanded to `$HOME`, so `logo = ~/art/logo.txt` works too. Relative
paths are resolved from the directory nexus runs in.

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

Override the tint with `--logo-color` (or `"logo_color"`): any SGR parameter
list works, so `--logo-color 38;5;208` paints a plain logo orange and
`--logo-color none` leaves it uncoloured. It applies to plain logos only —
fastfetch logos keep their own colours.

By default the logo is lifted into a height map and rendered as a spinning 3D
relief. Pass `--no-spin` (or set `"spin": false`) to draw it flat instead,
exactly as the source art, and exit. The logo sits on the left of the info
block; put it on the right with `--logo-side right` (or `"logo_side": "right"`).
When the terminal is too narrow, the info is stacked below the logo instead.

The space between the logo and the info block is set with `--gap <n>` (or
`"gap": <n>`), default `2`.
