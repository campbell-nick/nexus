/*
 * config.c - a tiny JSON config reader.
 *
 * The file lives at $XDG_CONFIG_HOME/nexus/nexus.json, or
 * ~/.config/nexus/nexus.json when XDG_CONFIG_HOME is unset. It is created with
 * a commented template on first run; an existing "key = value" nexus.conf is
 * migrated to JSON automatically.
 *
 * The reader is deliberately tolerant: // and block comments and trailing
 * commas are allowed, and values may be strings, numbers, booleans or arrays.
 * A key maps to a scalar (config_get) or, when it is an array, to every element
 * (config_get_all); config_get joins an array with ", ".
 *
 * --config <path> and --profile <name> point the reader at a different file.
 */
#define _GNU_SOURCE
#include "nexus.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static const char CONFIG_TEMPLATE[] =
    "// nexus configuration\n"
    "//\n"
    "// Flat JSON. Comments (// and /* */) and trailing commas are allowed.\n"
    "// Command-line flags always win over this file.\n"
    "{\n"
    "  // Logo: a file path, or a distro/logo name such as \"arch\".\n"
    "  // Empty = auto-detect. Paths may start with ~ (expanded to $HOME).\n"
    "  \"logo\": \"\",\n"
    "\n"
    "  // Which side of the info block the logo sits on: \"left\" or \"right\".\n"
    "  \"logo_side\": \"left\",\n"
    "\n"
    "  // Shading ramp, darkest to brightest. \"default\" is the built-in ramp\n"
    "  // (.,-~:;=!*#$@).\n"
    "  \"shading_chars\": \"default\",\n"
    "\n"
    "  // Rotation axis: \"x\", \"y\" or \"both\".\n"
    "  \"rotation\": \"both\",\n"
    "\n"
    "  // Animation speed multiplier.\n"
    "  \"speed\": 1.0,\n"
    "\n"
    "  // Logo scale and 3D depth.\n"
    "  \"size\": 1.0,\n"
    "  \"depth\": 1.0,\n"
    "\n"
    "  // Render height in rows; 0 = auto.\n"
    "  \"height\": 0,\n"
    "\n"
    "  // Stop after this many frames; 0 = run until a key is pressed.\n"
    "  \"frames\": 0,\n"
    "\n"
    "  // Show the system information beside the logo.\n"
    "  \"info\": true,\n"
    "\n"
    "  // Colour the logo.\n"
    "  \"color\": true,\n"
    "\n"
    "  // Show the 16-colour terminal palette under the info (hidden by --no-color).\n"
    "  \"palette\": true,\n"
    "\n"
    "  // Spin the logo in 3D. When false, the plain 2D logo is drawn instead.\n"
    "  \"spin\": true,\n"
    "\n"
    "  // --- Info block -------------------------------------------------------\n"
    "  // Which fields to show and in what order. Names: title, os, host, kernel,\n"
    "  // uptime, age, packages, nix, shell, cpu, gpu, memory, swap, disk, custom,\n"
    "  // palette. Use \"all\" for the built-in order. [] = built-in order.\n"
    "  \"fields\": [],\n"
    "\n"
    "  // Extra lines shown in the info block. {field} is replaced with that\n"
    "  // field's value (e.g. {os}, {kernel}, {user}).\n"
    "  \"custom\": [],\n"
    "\n"
    "  // Title format. {user} and {hostname} are replaced; \"none\" hides it.\n"
    "  // Empty uses the built-in user@hostname.\n"
    "  \"title\": \"\",\n"
    "\n"
    "  // Separator between a label and its value; \"none\" for a space.\n"
    "  \"separator\": \":\",\n"
    "\n"
    "  // Label and value colours as SGR parameters (e.g. \"1;35\"). Empty =\n"
    "  // default (labels are bold magenta, values uncoloured).\n"
    "  \"key_color\": \"1;35\",\n"
    "  \"value_color\": \"\"\n"
    "}\n";

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
    snprintf(buf, n, "%s/nexus/nexus.json", base);
    return 1;
  }
  const char *home = getenv("HOME");
  if (!home || !home[0])
    return 0;
  snprintf(buf, n, "%s/.config/nexus/nexus.json", home);
  return 1;
}

/* ------------------------------------------------------------ JSON reader */

static int is_ws(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/* Skip whitespace plus // line comments and block comments. */
static const char *skip_ws(const char *p, const char *end) {
  for (;;) {
    while (p < end && is_ws(*p))
      p++;
    if (p + 1 < end && p[0] == '/' && p[1] == '/') {
      p += 2;
      while (p < end && *p != '\n')
        p++;
      continue;
    }
    if (p + 1 < end && p[0] == '/' && p[1] == '*') {
      p += 2;
      while (p + 1 < end && !(p[0] == '*' && p[1] == '/'))
        p++;
      p = (p + 1 < end) ? p + 2 : end;
      continue;
    }
    break;
  }
  return p;
}

/* Scan past a string literal without decoding it. */
static const char *skip_string(const char *p, const char *end) {
  p++; /* opening quote */
  while (p < end) {
    if (*p == '\\') {
      p += 2;
      continue;
    }
    if (*p == '"')
      return p + 1;
    p++;
  }
  return end;
}

/* Decode a string literal, handling the usual escapes and \uXXXX (BMP only).
 * Returns the position just past the closing quote, or NULL on error. */
static const char *parse_string(const char *p, const char *end, char *out,
                                size_t outsz) {
  if (p >= end || *p != '"')
    return NULL;
  p++;
  size_t n = 0;
  while (p < end && *p != '"') {
    char c = *p++;
    if (c == '\\' && p < end) {
      char e = *p++;
      switch (e) {
      case 'n': c = '\n'; break;
      case 't': c = '\t'; break;
      case 'r': c = '\r'; break;
      case 'b': c = '\b'; break;
      case 'f': c = '\f'; break;
      case '"': c = '"'; break;
      case '\\': c = '\\'; break;
      case '/': c = '/'; break;
      case 'u': {
        unsigned cp = 0;
        for (int i = 0; i < 4 && p < end; i++) {
          int h = *p++;
          cp <<= 4;
          if (h >= '0' && h <= '9')
            cp |= (unsigned)(h - '0');
          else if (h >= 'a' && h <= 'f')
            cp |= (unsigned)(h - 'a' + 10);
          else if (h >= 'A' && h <= 'F')
            cp |= (unsigned)(h - 'A' + 10);
        }
        char tmp[3];
        int tl = 0;
        if (cp < 0x80) {
          tmp[tl++] = (char)cp;
        } else if (cp < 0x800) {
          tmp[tl++] = (char)(0xC0 | (cp >> 6));
          tmp[tl++] = (char)(0x80 | (cp & 0x3F));
        } else {
          tmp[tl++] = (char)(0xE0 | (cp >> 12));
          tmp[tl++] = (char)(0x80 | ((cp >> 6) & 0x3F));
          tmp[tl++] = (char)(0x80 | (cp & 0x3F));
        }
        for (int i = 0; i < tl; i++)
          if (n + 1 < outsz)
            out[n++] = tmp[i];
        continue;
      }
      default: c = e; break;
      }
    }
    if (n + 1 < outsz)
      out[n++] = c;
  }
  if (p >= end)
    return NULL;
  p++; /* closing quote */
  if (outsz)
    out[n] = '\0';
  return p;
}

/* Decode a scalar (string, number, true/false/null) into `out`. Returns the
 * position just past it, or NULL when the value is an array or object. */
static const char *parse_scalar(const char *p, const char *end, char *out,
                                size_t outsz) {
  p = skip_ws(p, end);
  if (p >= end)
    return NULL;
  if (*p == '"')
    return parse_string(p, end, out, outsz);
  if (*p == '{' || *p == '[')
    return NULL;
  const char *s = p;
  while (p < end && *p != ',' && *p != '}' && *p != ']' && !is_ws(*p))
    p++;
  size_t n = (size_t)(p - s);
  if (n == 0)
    return NULL;
  if (n == 4 && strncmp(s, "null", 4) == 0) {
    if (outsz)
      out[0] = '\0';
    return p;
  }
  if (n >= outsz)
    n = outsz - 1;
  memcpy(out, s, n);
  if (outsz)
    out[n] = '\0';
  return p;
}

/* Skip a whole value, including nested arrays and objects. */
static const char *skip_value(const char *p, const char *end) {
  p = skip_ws(p, end);
  if (p >= end)
    return p;
  if (*p == '"')
    return skip_string(p, end);
  if (*p == '{' || *p == '[') {
    char open = *p, close = (open == '{') ? '}' : ']';
    int depth = 0;
    while (p < end) {
      char c = *p;
      if (c == '"') {
        p = skip_string(p, end);
        continue;
      }
      if (c == open) {
        depth++;
        p++;
        continue;
      }
      if (c == close) {
        depth--;
        p++;
        if (depth == 0)
          return p;
        continue;
      }
      p++;
    }
    return end;
  }
  while (p < end && *p != ',' && *p != '}' && *p != ']' && !is_ws(*p))
    p++;
  return p;
}

/* Slurp the config file, then collect every value stored under `key`. An array
 * contributes one value per element; a scalar contributes one. Values are
 * copied into `out` (up to `max`); the return value is the total count. */
static int config_scan(const char *key, char (*out)[NEXUS_MAX_LINE_LEN],
                       int max) {
  char path[512];
  if (!config_path(path, sizeof(path)))
    return 0;

  FILE *fp = fopen(path, "r");
  if (!fp)
    return 0;
  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return 0;
  }
  long sz = ftell(fp);
  if (sz < 0 || sz > (1 << 20)) { /* 1 MiB is plenty for a config */
    fclose(fp);
    return 0;
  }
  rewind(fp);
  char *buf = malloc((size_t)sz + 1);
  if (!buf) {
    fclose(fp);
    return 0;
  }
  size_t rd = fread(buf, 1, (size_t)sz, fp);
  fclose(fp);
  buf[rd] = '\0';
  const char *end = buf + rd;

  int found = 0;
  const char *p = skip_ws(buf, end);
  if (p < end && *p == '{') {
    p++;
    for (;;) {
      p = skip_ws(p, end);
      if (p >= end || *p == '}')
        break;
      if (*p != '"') { /* tolerate stray characters */
        p++;
        continue;
      }
      char k[NEXUS_MAX_LINE_LEN];
      const char *q = parse_string(p, end, k, sizeof(k));
      if (!q)
        break;
      p = skip_ws(q, end);
      if (p >= end || *p != ':')
        break;
      p = skip_ws(p + 1, end);
      int match = (strcmp(k, key) == 0);

      if (p < end && *p == '[') {
        p++;
        for (;;) {
          p = skip_ws(p, end);
          if (p >= end)
            break;
          if (*p == ']') {
            p++;
            break;
          }
          char v[NEXUS_MAX_LINE_LEN];
          const char *after = parse_scalar(p, end, v, sizeof(v));
          if (after) {
            if (match) {
              if (out && found < max)
                snprintf(out[found], NEXUS_MAX_LINE_LEN, "%s", v);
              found++;
            }
            p = after;
          } else {
            p = skip_value(p, end);
          }
          p = skip_ws(p, end);
          if (p < end && *p == ',') {
            p++;
            continue;
          }
          if (p < end && *p == ']') {
            p++;
            break;
          }
        }
      } else {
        char v[NEXUS_MAX_LINE_LEN];
        const char *after = parse_scalar(p, end, v, sizeof(v));
        if (after) {
          if (match) {
            if (out && found < max)
              snprintf(out[found], NEXUS_MAX_LINE_LEN, "%s", v);
            found++;
          }
          p = after;
        } else {
          p = skip_value(p, end);
        }
      }

      p = skip_ws(p, end);
      if (p < end && *p == ',') {
        p++;
        continue;
      }
      if (p < end && *p == '}')
        break;
      if (p >= end)
        break;
    }
  }
  free(buf);
  return found;
}

int config_get(const char *key, char *out, size_t outlen) {
  if (!outlen)
    return 0;
  char vals[NEXUS_MAX_CUSTOM][NEXUS_MAX_LINE_LEN];
  int n = config_scan(key, vals, NEXUS_MAX_CUSTOM);
  if (n < 1)
    return 0;
  /* An array is joined with ", " so e.g. "fields": ["os","cpu"] reads as the
   * comma-separated list the rest of the program expects. */
  size_t used = 0;
  out[0] = '\0';
  for (int i = 0; i < n && i < NEXUS_MAX_CUSTOM; i++) {
    if (i) {
      if (used + 2 >= outlen)
        break;
      out[used++] = ',';
      out[used++] = ' ';
      out[used] = '\0';
    }
    size_t l = strlen(vals[i]);
    if (used + l >= outlen)
      l = outlen - 1 - used;
    memcpy(out + used, vals[i], l);
    used += l;
    out[used] = '\0';
  }
  return 1;
}

int config_get_all(const char *key, char (*out)[NEXUS_MAX_LINE_LEN], int max) {
  int n = config_scan(key, out, max);
  return n > max ? max : n;
}

/* ------------------------------------------------------------ migration */

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

/* Legacy "key = value" reader, used only to migrate an old nexus.conf. */
static int legacy_scan(const char *path, const char *key,
                       char (*out)[NEXUS_MAX_LINE_LEN], int max) {
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

/* Write a JSON string literal, escaping what JSON requires. */
static void json_escape(FILE *fp, const char *s) {
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    switch (c) {
    case '"': fputs("\\\"", fp); break;
    case '\\': fputs("\\\\", fp); break;
    case '\n': fputs("\\n", fp); break;
    case '\r': fputs("\\r", fp); break;
    case '\t': fputs("\\t", fp); break;
    default:
      if (c < 0x20)
        fprintf(fp, "\\u%04x", c);
      else
        fputc(c, fp);
    }
  }
}

static int key_is_bool(const char *k) {
  return !strcmp(k, "info") || !strcmp(k, "color") || !strcmp(k, "palette") ||
         !strcmp(k, "spin");
}

static int key_is_number(const char *k) {
  return !strcmp(k, "speed") || !strcmp(k, "size") || !strcmp(k, "depth") ||
         !strcmp(k, "height") || !strcmp(k, "frames");
}

/* True when the text is a plain decimal number, so it can be emitted unquoted. */
static int looks_like_number(const char *s) {
  if (*s == '-' || *s == '+')
    s++;
  int dots = 0, digits = 0;
  for (; *s; s++) {
    if (*s == '.') {
      if (++dots > 1)
        return 0;
    } else if (*s >= '0' && *s <= '9') {
      digits++;
    } else {
      return 0;
    }
  }
  return digits > 0;
}

/* Emit a scalar value, keeping booleans and numbers as native JSON types. */
static void emit_json_value(FILE *fp, const char *key, const char *v) {
  if (key_is_bool(key) &&
      (strcasecmp(v, "true") == 0 || strcasecmp(v, "false") == 0)) {
    fputs(strcasecmp(v, "true") == 0 ? "true" : "false", fp);
  } else if (key_is_number(key) && looks_like_number(v)) {
    fputs(v, fp);
  } else {
    fputc('"', fp);
    json_escape(fp, v);
    fputc('"', fp);
  }
}

/* Translate an old nexus.conf into nexus.json. Returns 1 on success. */
static int config_migrate(const char *oldpath, const char *newpath) {
  FILE *out = fopen(newpath, "w");
  if (!out)
    return 0;

  fprintf(out, "// Migrated from %s\n", oldpath);
  fputs("// Flat JSON; comments (// and /* */) and trailing commas are allowed.\n",
        out);
  fputs("{\n", out);

  static const char *keys[] = {
      "logo",      "logo_side", "shading_chars", "rotation", "speed",
      "size",      "depth",     "height",        "frames",   "info",
      "color",     "palette",   "spin",          "fields",   "title",
      "separator", "key_color", "value_color",
  };
  int first = 1;
  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
    char v[NEXUS_MAX_LINE_LEN];
    if (legacy_scan(oldpath, keys[i], &v, 1) < 1 || !v[0])
      continue;
    fputs(first ? "  " : ",\n  ", out);
    first = 0;
    fprintf(out, "\"%s\": ", keys[i]);
    emit_json_value(out, keys[i], v);
  }

  char custom[NEXUS_MAX_CUSTOM][NEXUS_MAX_LINE_LEN];
  int nc = legacy_scan(oldpath, "custom", custom, NEXUS_MAX_CUSTOM);
  int any = 0;
  for (int i = 0; i < nc && i < NEXUS_MAX_CUSTOM; i++)
    if (custom[i][0])
      any = 1;
  if (any) {
    fputs(first ? "  " : ",\n  ", out);
    first = 0;
    fputs("\"custom\": [", out);
    int w = 0;
    for (int i = 0; i < nc && i < NEXUS_MAX_CUSTOM; i++) {
      if (!custom[i][0])
        continue;
      if (w++)
        fputs(", ", out);
      fputc('"', out);
      json_escape(out, custom[i]);
      fputc('"', out);
    }
    fputc(']', out);
  }

  fputs("\n}\n", out);
  fclose(out);
  return 1;
}

/* Return the path of the legacy nexus.conf sitting beside `path`. */
static void legacy_path_beside(const char *path, char *out, size_t n) {
  snprintf(out, n, "%s", path);
  char *slash = strrchr(out, '/');
  char *dot = strrchr(out, '.');
  if (dot && (!slash || dot > slash))
    *dot = '\0';
  size_t len = strlen(out);
  snprintf(out + len, n - len, ".conf");
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

  /* Migrate an old nexus.conf if one is present, keeping it as a .bak. */
  char oldpath[512];
  legacy_path_beside(path, oldpath, sizeof(oldpath));
  FILE *old = fopen(oldpath, "r");
  if (old) {
    fclose(old);
    if (config_migrate(oldpath, path)) {
      char bak[512];
      snprintf(bak, sizeof(bak), "%s.bak", oldpath);
      rename(oldpath, bak);
      return;
    }
  }

  fp = fopen(path, "w");
  if (!fp)
    return;
  fputs(CONFIG_TEMPLATE, fp);
  fclose(fp);
}
