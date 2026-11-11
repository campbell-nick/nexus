/*
 * util.c - small text / terminal helpers shared across nexus.
 */
#define _GNU_SOURCE
#include "nexus.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Byte length of a UTF-8 sequence given its leading byte. */
int nx_utf8_len(unsigned char c) {
  if (c < 0x80)
    return 1;
  if ((c & 0xE0) == 0xC0)
    return 2;
  if ((c & 0xF0) == 0xE0)
    return 3;
  if ((c & 0xF8) == 0xF0)
    return 4;
  return 1;
}

/* Bytes consumed by an ANSI escape at p (ESC [ params letter), or 0. */
int nx_skip_ansi(const char *p) {
  if (p[0] != '\033' || p[1] != '[')
    return 0;
  int i = 2;
  while (p[i] && ((p[i] >= '0' && p[i] <= '9') || p[i] == ';'))
    i++;
  if (p[i])
    i++;
  return i;
}

/* Visible columns, ignoring ANSI escapes (each codepoint counts as one). */
int nx_visible_width(const char *s) {
  int w = 0;
  while (*s) {
    int a = nx_skip_ansi(s);
    if (a) {
      s += a;
      continue;
    }
    int len = nx_utf8_len((unsigned char)*s);
    while (len > 0 && *s) {
      s++;
      len--;
    }
    w++;
  }
  return w;
}

/* Copy s into p clipped to max_cols visible columns. ANSI passes through;
 * max_cols < 0 means no limit. Appends "..." when clipped. */
char *nx_emit_clipped(char *p, char *end, const char *s, int max_cols) {
  int total_w = 0;
  const char *t = s;
  while (*t) {
    int a = nx_skip_ansi(t);
    if (a) {
      t += a;
      continue;
    }
    t += nx_utf8_len((unsigned char)*t);
    total_w++;
  }
  int need_clip = (max_cols >= 0 && total_w > max_cols);
  int limit = max_cols;
  if (need_clip && max_cols >= 6)
    limit = max_cols - 3;

  int w = 0, had_ansi = 0;
  while (*s && p + 8 < end) {
    int a = nx_skip_ansi(s);
    if (a) {
      if (p + a + 8 >= end)
        break;
      memcpy(p, s, a);
      p += a;
      s += a;
      had_ansi = 1;
      continue;
    }
    if (limit >= 0 && w >= limit)
      break;
    int len = nx_utf8_len((unsigned char)*s);
    int actual = 0;
    while (actual < len && s[actual])
      actual++;
    memcpy(p, s, actual);
    p += actual;
    s += actual;
    w++;
  }
  if (need_clip && max_cols >= 6 && p + 3 < end) {
    memcpy(p, "...", 3);
    p += 3;
  }
  if (had_ansi && need_clip && p + 4 < end) {
    memcpy(p, "\033[0m", 4);
    p += 4;
  }
  return p;
}

int nx_parse_number(const char *option, const char *text, float min, float max,
                    float *out) {
  char *end;
  errno = 0;
  float value = strtof(text, &end);
  if (end == text || *end || errno == ERANGE || !isfinite(value) || value < min ||
      value > max) {
    fprintf(stderr,
            "nexus: %s requires a finite number from %g to %g (got '%s')\n",
            option, min, max, text);
    return 0;
  }
  *out = value;
  return 1;
}

int nx_parse_integer(const char *option, const char *text, int min, int max,
                     int *out) {
  char *end;
  errno = 0;
  long value = strtol(text, &end, 10);
  if (end == text || *end || errno == ERANGE || value < min || value > max) {
    fprintf(stderr, "nexus: %s requires an integer from %d to %d (got '%s')\n",
            option, min, max, text);
    return 0;
  }
  *out = (int)value;
  return 1;
}

/* Read the first line of a small pseudo-file, trimming the newline. */
int nx_read_first_line(const char *path, char *out, int outlen) {
  out[0] = '\0';
  FILE *fp = fopen(path, "r");
  if (!fp)
    return 0;
  if (fgets(out, outlen, fp)) {
    int len = (int)strlen(out);
    while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r'))
      out[--len] = '\0';
  }
  fclose(fp);
  return out[0] != '\0';
}
