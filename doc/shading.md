# Shading

Brightness is mapped onto a ramp, one character per cell. The built-in ramp is
`.,-~:;=!*#$@`; override it on the command line:

```
./nexus --shading-chars '#*.'
```

or in the config file (see [configuration.md](configuration.md)):

```json
{ "shading_chars": "o" }
```

The ramp is UTF-8 aware, so block or dot glyphs work too:

```
./nexus --shading-chars ' .-+*#%M@'
./nexus --shading-chars ' ░▒▓█'
```

A **leading space** is worth keeping: it makes the darkest cells drop out to the
terminal background, which reads as the empty space in a logo like a black hole.

The ramp is applied in the spinning 3D view. `--no-spin` draws the source art
verbatim, so it is unaffected by `shading_chars`.
