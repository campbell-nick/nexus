/*
 * config.c - a tiny "key = value" config reader.
 *
 * The file lives at $XDG_CONFIG_HOME/nexus/nexus.conf, or
 * ~/.config/nexus/nexus.conf when XDG_CONFIG_HOME is unset. It is created with
 * a commented template on first run. Blank lines and lines starting with '#'
 * are ignored; the value is everything after the first '=', trimmed, with one
 * pair of matching surrounding quotes removed. A key may appear more than once
 * (config_get_all collects every value); config_get returns the first.
 *
 * --config <path> and --profile <name> point the reader at a different file.
 */
#define _GNU_SOURCE
#include "nexus.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char CONFIG_TEMPLATE[] =
    "# nexus configuration\n"
    "#\n"
    "# One \"key = value\" per line; blank lines and lines starting with '#' are\n"
    "# ignored. Command-line flags always win over this file.\n"
    "\n"
    "# Logo: a file path, or a distro/logo name such as \"arch\". Empty = auto.\n"
    "logo =\n"
    "\n"
    "# Which side of the info block the logo sits on: left or right.\n"
    "logo_side = left\n"
    "\n"
    "# Shading ramp, darkest to brightest. \"default\" is the built-in ramp\n"
    "# (.,-~:;=!*#$@).\n"
    "shading_chars = default\n"
    "\n"
    "# Rotation axis: x, y or both.\n"
    "rotation = both\n"
    "\n"
    "# Animation speed multiplier.\n"
    "speed = 1.0\n"
    "\n"
    "# Logo scale and 3D depth.\n"
    "size = 1.0\n"
    "depth = 1.0\n"
    "\n"
    "# Render height in rows; 0 = auto.\n"
    "height = 0\n"
    "\n"
    "# Stop after this many frames; 0 = run until a key is pressed.\n"
    "frames = 0\n"
    "\n"
    "# Show the system information beside the logo.\n"
    "info = true\n"
    "\n"
    "# Colour the logo.\n"
    "color = true\n"
    "\n"
    "# Show the 16-colour terminal palette under the info (hidden by --no-color).\n"
    "palette = true\n"
    "\n"
    "# Spin the logo in 3D. When false, the plain 2D logo is drawn instead.\n"
    "spin = true\n"
    "\n"
    "# --- Info block ---------------------------------------------------------\n"
    "#\n"
    "# Which fields to show and in what order. Names: title, os, host, kernel,\n"
    "# uptime, age, packages, shell, cpu, gpu, memory, swap, disk, custom,\n"
    "# palette. Use \"all\" for the built-in order. Empty = built-in order.\n"
    "# fields = os, kernel, cpu, memory, custom, palette\n"
    "fields =\n"
    "\n"
    "# Extra lines shown in the info block. May be repeated; {field} is\n"
    "# replaced with that field's value (e.g. {os}, {kernel}, {user}).\n"
    "# custom = Welcome back!\n"
    "custom =\n"
    "\n"
    "# Title format. {user} and {hostname} are replaced; \"none\" hides it.\n"
    "# Empty uses the built-in user@hostname.\n"
    "title =\n"
    "\n"
    "# Separator between a label and its value; \"none\" for a space.\n"
    "separator = :\n"
    "\n"
    "# Label and value colours as SGR parameters (e.g. 1;35). Empty = default\n"
    "# (labels are bold magenta, values uncoloured).\n"
    "key_color = 1;35\n"
    "value_color =\n";

/* A --config or --profile argument replaces the default file path. */
static char config_override[512];
static int config_override_set = 0;

void config_set_path(const char *path) {
  if (!path || !path[0])
    return;
  snprintf(config_override, sizeof(config_override), "%s", path);
  config_override_set = 1;
}

/* Build the config file path. Returns 0 when no home directory is known. */
static int config_path(char *buf, size_t n) {
  if (config_override_set) {
    snprintf(buf, n, "%s", config_override);
    return 1;
  }
  const char *base = getenv("XDG_CONFIG_HOME");
  if (base && base[0]) {
    snprintf(buf, n, "%s/nexus/nexus.conf", base);
    return 1;
  }
  const char *home = getenv("HOME");
  if (!home || !home[0])
    return 0;
  snprintf(buf, n, "%s/.config/nexus/nexus.conf", home);
  return 1;
}

/* Trim leading and trailing blanks / newlines in place. */
static char *trim(char *s) {
  while (*s == ' ' || *s == '\t')
    s++;
  char *end = s + strlen(s);
  while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' ||
                     end[-1] == '\r'))
    *--end = '\0';
  return s;
}

/* Strip one pair of matching surrounding quotes, in place. */
static char *unquote(char *v) {
  size_t len = strlen(v);
  if (len >= 2 && (v[0] == '"' || v[0] == '\'') && v[len - 1] == v[0]) {
    v[len - 1] = '\0';
    v++;
  }
  return v;
}

void config_ensure(void) {
  char path[512];
  if (!config_path(path, sizeof(path)))
    return;

  FILE *fp = fopen(path, "r");
  if (fp) { /* already there */
    fclose(fp);
    return;
  }

  /* Create the containing directory (and its parent, usually ~/.config). */
  char dir[512];
  snprintf(dir, sizeof(dir), "%s", path);
  char *slash = strrchr(dir, '/');
  if (slash) {
    *slash = '\0';
    char parent[512];
    snprintf(parent, sizeof(parent), "%s", dir);
    char *pslash = strrchr(parent, '/');
    if (pslash) {
      *pslash = '\0';
      mkdir(parent, 0755);
    }
    mkdir(dir, 0755);
  }

  fp = fopen(path, "w");
  if (!fp)
    return;
  fputs(CONFIG_TEMPLATE, fp);
  fclose(fp);
}

/* Walk the config file, invoking `cb` for every value of `key`. Returns the
 * number of matches. When `out` is non-NULL each value is also copied there. */
static int config_scan(const char *key, char (*out)[NEXUS_MAX_LINE_LEN],
                       int max) {
  char path[512];
  if (!config_path(path, sizeof(path)))
    return 0;

  FILE *fp = fopen(path, "r");
  if (!fp)
    return 0;

  char line[512];
  int found = 0;
  while (fgets(line, sizeof(line), fp)) {
    char *p = trim(line);
    if (*p == '\0' || *p == '#')
      continue;
    char *eq = strchr(p, '=');
    if (!eq)
      continue;
    *eq = '\0';
    if (strcmp(trim(p), key) != 0)
      continue;
    if (out && found < max)
      snprintf(out[found], NEXUS_MAX_LINE_LEN, "%s", unquote(trim(eq + 1)));
    found++;
  }
  fclose(fp);
  return found;
}

int config_get(const char *key, char *out, size_t outlen) {
  char value[NEXUS_MAX_LINE_LEN];
  if (config_scan(key, &value, 1) < 1)
    return 0;
  snprintf(out, outlen, "%s", value);
  return 1;
}

int config_get_all(const char *key, char (*out)[NEXUS_MAX_LINE_LEN], int max) {
  int n = config_scan(key, out, max);
  return n > max ? max : n;
}
