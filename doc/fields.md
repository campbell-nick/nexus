# Info fields

The default field set is deliberately small and cheap to gather:

```
title, os, host, kernel, uptime, age, packages, shell, cpu, gpu, memory, swap, disk, palette
```

`title` is the `user@host` line with a separator rule under it. `age` is how
long ago the distribution was installed, taken from the root filesystem's
birth time; it is omitted when the filesystem does not report one. Package
counts come from emerge, pacman, dpkg, rpm, xbps, apk, eopkg or nix, whichever
is present.

`nix` is a separate, opt-in field that counts Nix packages per profile the way
fastfetch does: it walks each profile's store closure and keeps the paths that
look like real packages (a version, not a `-dev`/`-bin`/`-doc`/`-man`/`-info`
output, and not the system derivation). Counts are labelled `nix-system`,
`nix-user` and `nix-default`, and profiles that are absent are left out:

```
Nix: 5 (nix-user), 61 (nix-default)
```

Because it is not in the default order, plain `nexus` is unchanged and there is
no duplication on NixOS unless you ask for it. Add it below `packages` with:

```
./nexus --fields "os, packages, nix, shell, cpu, memory"
```

Choose and order the fields with `--fields` (or the `fields` key):

```
./nexus --fields "os, kernel, cpu, memory, custom, palette"
```

Custom lines are added with the `custom` array in the config file (there is no
command-line flag for them). Each may reference any field, plus `{user}` and
`{hostname}`:

```json
// ~/.config/nexus/nexus.json
"custom": [
  "Welcome back, {user}!",
  "uptime: {uptime}"
]
```

The title, separator and colours are configurable too:

```
./nexus --title "{user} on {hostname}" --separator "->" --key-color "1;36"
./nexus --title none            # hide the title line
```

Below the fields, nexus draws the 16 standard terminal colours as two rows of
eight blocks (place it with `palette` in `--fields`). Hide it with
`--no-palette` (or `"palette": false`); it is also hidden by `--no-info` and
`--no-color`.

See also: [options.md](options.md), [configuration.md](configuration.md).
