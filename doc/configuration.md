# Configuration

On first run nexus writes `~/.config/nexus/nexus.json` (honouring
`XDG_CONFIG_HOME`) with a commented template. It is a flat JSON object; `//` and
`/* */` comments and trailing commas are allowed, so the template stays
self-documenting. Every command-line option has a matching key, and **flags
always win over the file**:

| Key             | Values                          | Equivalent flag       |
| --------------- | ------------------------------- | --------------------- |
| `logo`          | file path (`~` = `$HOME`) or distro/logo name | `--logo`    |
| `shading_chars` | any ramp; `default` for built-in| `--shading-chars`     |
| `rotation`      | `x`, `y` or `both`              | `--rotate-x`/`--rotate-y` |
| `speed`         | number, e.g. `1.0`              | `--speed`             |
| `size`          | number, e.g. `1.0`              | `--size`              |
| `depth`         | number, e.g. `1.0`              | `--depth`             |
| `height`        | rows; `0` = auto                | `--height`            |
| `gap`           | spaces; `2` = default           | `--gap`               |
| `frames`        | count; `0` = until a keypress   | `--frames`            |
| `info`          | `true`/`false`                  | `--no-info` (inverse) |
| `color`         | `true`/`false`                  | `--no-color` (inverse)|
| `palette`       | `true`/`false`                  | `--no-palette` (inverse)|
| `spin`          | `true`/`false`                  | `--no-spin` (inverse) |
| `fields`        | field names, as a string or array | `--fields`          |
| `custom`        | text or array of text; `{field}` expands | —            |
| `title`         | format with `{user}`/`{hostname}`, or `none` | `--title` |
| `separator`     | glyph, or `none`                | `--separator`         |
| `key_color`     | SGR parameters, e.g. `1;35`     | `--key-color`         |
| `value_color`   | SGR parameters                  | `--value-color`       |
| `logo_side`     | `left` or `right`               | `--logo-side`         |
| `logo_color`    | SGR parameters, e.g. `38;5;208`; `none` = no colour | `--logo-color` |

Boolean keys take native `true`/`false`, or the strings `yes`/`no`, `on`/`off`
and `1`/`0`. A `default` value (or a value equal to the built-in default) leaves
the built-in behaviour in place. An unknown value prints a warning and keeps the
built-in default.

`fields` and `custom` may be JSON arrays, which reads more naturally than one
comma-separated string:

```json
{
  "logo": "~/art/logo.txt",
  "fields": ["os", "kernel", "custom", "palette"],
  "custom": ["Welcome back, {user}!", "uptime: {uptime}"],
  "spin": false
}
```

A `nexus.conf` from an earlier version is migrated to `nexus.json`
automatically on first run and kept as `nexus.conf.bak`.

A profile is just another config file: `--profile work` reads
`~/.config/nexus/work.json`, and `--config <path>` reads an arbitrary file. The
usual precedence applies, so flags still win over the file.

See also: [options.md](options.md), [fields.md](fields.md),
[logos.md](logos.md), [shading.md](shading.md).
