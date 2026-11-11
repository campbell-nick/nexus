/*
 * config.c - a tiny "key = value" config reader.
 *
 * The file lives at $XDG_CONFIG_HOME/nexus/nexus.conf, or
 * ~/.config/nexus/nexus.conf when XDG_CONFIG_HOME is unset. It is created with
 * a commented template on first run. Blank lines and lines starting with '#'
 * are ignored; the value is everything after the first '=', trimmed, with one
 * pair of matching surrounding quotes removed.
 */
#define _GNU_SOURCE
#include "nexus.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char CONFIG_TEMPLATE[] =
    "# nexus configuration\n"
    "#\n"
    "# Shading ramp used to draw the logo, from darkest to brightest; each\n"
    "# character (or UTF-8 glyph) is one step. Use \"default\" for the\n"
    "# built-in ramp (.,-~:;=!*#$@).\n"
    "shading_chars = default\n";

/* Build the config file path. Returns 0 when no home directory is known. */
static int config_path(char *buf, size_t n) {
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

const char *config_get(const char *key) {
  static char value[512];
  char path[512];
  if (!config_path(path, sizeof(path)))
    return NULL;

  FILE *fp = fopen(path, "r");
  if (!fp)
    return NULL;

  char line[512];
  const char *found = NULL;
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

    char *v = trim(eq + 1);
    size_t vlen = strlen(v);
    if (vlen >= 2 && (v[0] == '"' || v[0] == '\'') && v[vlen - 1] == v[0]) {
      v[vlen - 1] = '\0';
      v++;
    }
    snprintf(value, sizeof(value), "%s", v);
    found = value;
    break;
  }
  fclose(fp);
  return found;
}
