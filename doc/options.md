# Command-line options

Every option can also be set in the config file; **flags always win over the
file**, which in turn wins over the built-in defaults. See
[configuration.md](configuration.md) for the matching keys.

| Flag | Description |
|------|-------------|
| `-l`, `--logo <name\|path>` | Show a distro logo by name (e.g. `arch`), or load a logo file |
| `--rotate-x` | Lock rotation to the X axis only |
| `--rotate-y` | Lock rotation to the Y axis only |
| `-s`, `--speed <float>` | Speed multiplier (default `1.0`) |
| `--size <float>` | Scale the logo (e.g. `2.0` for double size) |
| `--depth <float>` | Scale the 3D depth (default `1.0`) |
| `--height <n>` | Override the render height in rows |
| `--gap <n>` | Spaces between the logo and the info block (default `2`) |
| `--no-info` | Just the logo, no system info |
| `--no-color` | Disable logo coloring |
| `--no-palette` | Hide the 16-colour terminal palette |
| `--no-spin` | Draw the plain 2D logo instead of the spinning 3D one |
| `--frames <n>` | Stop after `n` frames (default: run until a keypress) |
| `--shading-chars <str>` | Custom shading ramp (UTF-8 aware) |
| `--fields <list>` | Info fields to show, in order; `all` for the default order |
| `--title <format>` | Title line; `{user}`/`{hostname}`; `none` hides it |
| `--separator <glyph>` | Label/value separator; `none` for a space |
| `--key-color <sgr>` | SGR for labels (default `1;35`); `none` for no colour |
| `--value-color <sgr>` | SGR for values (default: terminal default) |
| `--logo-side <left\|right>` | Which side the logo sits on (default `left`) |
| `--logo-color <sgr>` | Logo tint as SGR params (e.g. `38;5;208` for orange); `none` for no colour |
| `--config <path>` | Read this config file instead of the default |
| `--profile <name>` | Read `~/.config/nexus/<name>.json` |
| `-h`, `--help` | Show help |
| `-V`, `--version` | Show version |
