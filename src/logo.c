/*
 * logo.c - loading, decoding and colouring of the ASCII/Unicode logo.
 *
 * A logo is stored as rows of codepoints, each with an optional ANSI
 * foreground colour. Sources, in order of preference: an explicit file path
 * (--logo), ~/.config/nexus/logo.txt, fastfetch's built-in library (when it is
 * installed), the embedded core set from distros.c, then a plain Tux and
 * finally the blocky "N".
 */
#define _GNU_SOURCE
#include "nexus.h"

#include <stdlib.h>
#include <string.h>

/* Raw rows as read from disk, before codepoint decoding. */
static char logo_data[NEXUS_MAX_LOGO_ROWS][512];

char logo_cells[NEXUS_MAX_LOGO_ROWS][NEXUS_MAX_LOGO_COLS][5];
int logo_cell_color[NEXUS_MAX_LOGO_ROWS][NEXUS_MAX_LOGO_COLS];
int logo_cell_counts[NEXUS_MAX_LOGO_ROWS];
int logo_rows = 0;
int logo_cols = 0;
int logo_has_ansi = 0;
char g_file_distro[64] = "";
char g_distro_id_like[64] = "";

const char *g_color_inner = "\033[1;37m"; /* front/back faces */
const char *g_color_outer = "\033[1;35m"; /* extruded sides  */

/* ---------------------------------------------------------------- weights */
/* Rough visual density of a glyph, 0 (space) .. 1 (solid). This is what turns
 * the flat art into a relief: heavy glyphs push the surface up, light ones
 * sink it. */
float logo_char_weight(const char *ch) {
  if ((unsigned char)ch[0] < 0x80) {
    switch (ch[0]) {
    case 'M': return 1.00f;
    case 'N': return 0.88f;
    case 'm': return 0.76f;
    case 'd': return 0.66f;
    case 'h': return 0.56f;
    case 'b': return 0.56f;
    case 'y': return 0.46f;
    case 'o': return 0.38f;
    case 'n': return 0.38f;
    case 's': return 0.30f;
    case '+': return 0.22f;
    case '=': return 0.22f;
    case ':': return 0.18f;
    case '-': return 0.14f;
    case '/': return 0.12f;
    case '.': return 0.10f;
    case '`': return 0.08f;
    case '\'': return 0.06f;
    case ' ': return 0.0f;
    default:
      if (ch[0] >= 'A' && ch[0] <= 'Z') return 0.80f;
      if (ch[0] >= 'a' && ch[0] <= 'z') return 0.50f;
      if (ch[0] >= '0' && ch[0] <= '9') return 0.40f;
      return 0.15f;
    }
  }

  /* Unicode block elements, matched on their raw bytes. */
  if (memcmp(ch, "\xe2\x96\x88", 3) == 0) return 1.00f; /* full     */
  if (memcmp(ch, "\xe2\x96\x93", 3) == 0) return 0.75f; /* dark     */
  if (memcmp(ch, "\xe2\x96\x92", 3) == 0) return 0.50f; /* medium   */
  if (memcmp(ch, "\xe2\x96\x91", 3) == 0) return 0.25f; /* light    */
  if (memcmp(ch, "\xe2\x96\x80", 3) == 0) return 0.50f; /* upper    */
  if (memcmp(ch, "\xe2\x96\x84", 3) == 0) return 0.50f; /* lower    */
  if (memcmp(ch, "\xe2\x96\x8c", 3) == 0) return 0.50f; /* left     */
  if (memcmp(ch, "\xe2\x96\x90", 3) == 0) return 0.50f; /* right    */
  if (memcmp(ch, "\xe2\x96\x9b", 3) == 0) return 0.75f; /* 3/4 bl   */
  if (memcmp(ch, "\xe2\x96\x9c", 3) == 0) return 0.75f;
  if (memcmp(ch, "\xe2\x96\x99", 3) == 0) return 0.75f;
  if (memcmp(ch, "\xe2\x96\x9f", 3) == 0) return 0.75f;
  if (memcmp(ch, "\xe2\x96\x98", 3) == 0) return 0.25f; /* 1/4      */
  if (memcmp(ch, "\xe2\x96\x9d", 3) == 0) return 0.25f;
  if (memcmp(ch, "\xe2\x96\x96", 3) == 0) return 0.25f;
  if (memcmp(ch, "\xe2\x96\x97", 3) == 0) return 0.25f;

  /* Box drawing: thin lines. */
  if ((unsigned char)ch[0] == 0xe2 &&
      ((unsigned char)ch[1] == 0x94 || (unsigned char)ch[1] == 0x95))
    return 0.20f;

  /* Braille: weight by dot count. */
  if ((unsigned char)ch[0] == 0xe2 && (unsigned char)ch[1] >= 0xa0 &&
      (unsigned char)ch[1] <= 0xa3) {
    unsigned char b = (unsigned char)ch[2];
    int dots = 0;
    while (b) {
      dots += b & 1;
      b >>= 1;
    }
    return dots / 8.0f;
  }

  return 0.30f;
}

/* -------------------------------------------------------------- decoding */
/* Split one raw row into codepoints, tracking the active SGR foreground. */
static void decode_row(int row) {
  const char *p = logo_data[row];
  int col = 0;
  int cur_color = 0;
  while (*p && col < NEXUS_MAX_LOGO_COLS) {
    if (p[0] == '\033' && p[1] == '[') {
      int i = 2;
      int num = 0, has_num = 0;
      while (p[i] && ((p[i] >= '0' && p[i] <= '9') || p[i] == ';' ||
                      p[i] == '?')) {
        if (p[i] >= '0' && p[i] <= '9') {
          num = num * 10 + (p[i] - '0');
          has_num = 1;
        } else { /* separator (';' or a private-mode '?') */
          if (has_num && ((num >= 30 && num <= 37) || num == 39 ||
                          (num >= 90 && num <= 97)))
            cur_color = num;
          if (has_num && (num == 0 || num == 22))
            cur_color = 0;
          num = 0;
          has_num = 0;
        }
        i++;
      }
      if (has_num && ((num >= 30 && num <= 37) || num == 39 ||
                      (num >= 90 && num <= 97)))
        cur_color = num;
      if (has_num && num == 0)
        cur_color = 0;
      if (p[i])
        i++;
      if (cur_color > 0)
        logo_has_ansi = 1;
      p += i;
      continue;
    }
    int len = nx_utf8_len((unsigned char)*p);
    int actual = 0;
    while (actual < len && p[actual])
      actual++;
    memcpy(logo_cells[row][col], p, actual);
    logo_cells[row][col][actual] = '\0';
    logo_cell_color[row][col] = cur_color;
    col++;
    p += actual;
  }
  logo_cell_counts[row] = col;
  if (col > logo_cols)
    logo_cols = col;
}

void logo_process(void) {
  logo_cols = 0;
  for (int r = 0; r < logo_rows; r++)
    decode_row(r);
}

/* --------------------------------------------------------------- loading */
static void trim_row(char *buf) {
  int len = (int)strlen(buf);
  while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
    buf[--len] = '\0';
}

/* Load a logo from an explicit path. A leading "# distro: <name>" line sets
 * the colour scheme and is not drawn. */
int logo_load_path(const char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp)
    return 0;

  logo_rows = 0;
  g_file_distro[0] = '\0';
  char buf[512];
  while (logo_rows < NEXUS_MAX_LOGO_ROWS && fgets(buf, sizeof(buf), fp)) {
    trim_row(buf);
    int len = (int)strlen(buf);
    if (logo_rows == 0 && strncmp(buf, "# distro:", 9) == 0) {
      char *val = buf + 9;
      while (*val == ' ')
        val++;
      strncpy(g_file_distro, val, sizeof(g_file_distro) - 1);
      g_file_distro[sizeof(g_file_distro) - 1] = '\0';
      continue;
    }
    if (len == 0 && logo_rows == 0)
      continue;
    memcpy(logo_data[logo_rows], buf, len + 1);
    logo_rows++;
  }
  fclose(fp);
  while (logo_rows > 0 && logo_data[logo_rows - 1][0] == '\0')
    logo_rows--;
  return logo_rows > 0;
}

int logo_load_file(void) {
  const char *home = getenv("HOME");
  if (!home)
    return 0;
  char path[512];
  snprintf(path, sizeof(path), "%s/.config/nexus/logo.txt", home);
  return logo_load_path(path);
}

/* Load one of the embedded plain-text logos from distros.c. */
int logo_load_builtin(const char *id) {
  int rows = 0;
  const char *const *art = logo_builtin_find(id, &rows);
  if (!art || rows <= 0)
    return 0;
  if (rows > NEXUS_MAX_LOGO_ROWS)
    rows = NEXUS_MAX_LOGO_ROWS;
  logo_rows = rows;
  for (int i = 0; i < rows; i++) {
    int len = (int)strlen(art[i]);
    if (len > (int)sizeof(logo_data[0]) - 1)
      len = (int)sizeof(logo_data[0]) - 1;
    memcpy(logo_data[i], art[i], len);
    logo_data[i][len] = '\0';
  }
  return 1;
}

/* A row holding only whitespace and ANSI escapes (cursor show/hide and the
 * like) carries no glyphs and should not be drawn. */
static int ff_row_blank(const char *s) {
  while (*s) {
    if (s[0] == '\033' && s[1] == '[') {
      int i = 2;
      while (s[i] && (s[i] == '?' || s[i] == '>' || s[i] == '!' ||
                      (s[i] >= '0' && s[i] <= '9') || s[i] == ';'))
        i++;
      if (s[i])
        i++;
      s += i;
      continue;
    }
    if (*s != ' ' && *s != '\t')
      return 0;
    s++;
  }
  return 1;
}

/* Borrow fastfetch's built-in logo library. name may be NULL, in which case
 * fastfetch auto-detects the distribution. Returns 1 on success. */
int logo_load_fastfetch(const char *name) {
  char cmd[256];
  if (name && *name)
    snprintf(cmd, sizeof(cmd),
             "fastfetch -c none -l '%s' -s break --pipe false 2>/dev/null",
             name);
  else
    snprintf(cmd, sizeof(cmd),
             "fastfetch -c none -s break --pipe false 2>/dev/null");

  FILE *fp = popen(cmd, "r");
  if (!fp)
    return 0;

  char buf[2048];
  int rows = 0;
  while (rows < NEXUS_MAX_LOGO_ROWS && fgets(buf, sizeof(buf), fp)) {
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r' ||
                       buf[len - 1] == ' ' || buf[len - 1] == '\t'))
      buf[--len] = '\0';
    if (rows == 0 && ff_row_blank(buf))
      continue;
    if (len > (int)sizeof(logo_data[0]) - 1)
      len = (int)sizeof(logo_data[0]) - 1;
    memcpy(logo_data[rows], buf, len);
    logo_data[rows][len] = '\0';
    rows++;
  }
  int status = pclose(fp);

  while (rows > 0 && ff_row_blank(logo_data[rows - 1]))
    rows--;

  if (rows <= 0 || status != 0)
    return 0;
  logo_rows = rows;
  return 1;
}

/* ------------------------------------------------------------- detection */
static int parse_os_release_val(const char *buf, int prefix_len, char *out,
                                int maxlen) {
  int len = (int)strlen(buf);
  char tmp[256];
  if (len - prefix_len >= (int)sizeof(tmp))
    return 0;
  memcpy(tmp, buf + prefix_len, len - prefix_len + 1);
  len = (int)strlen(tmp);
  while (len > 0 && (tmp[len - 1] == '\n' || tmp[len - 1] == '\r'))
    tmp[--len] = '\0';
  char *val = tmp;
  if (*val == '"' || *val == '\'')
    val++;
  len = (int)strlen(val);
  if (len > 0 && (val[len - 1] == '"' || val[len - 1] == '\''))
    val[--len] = '\0';
  if (len > 0 && len < maxlen) {
    memcpy(out, val, len + 1);
    return 1;
  }
  return 0;
}

int logo_detect_distro(char *out, int maxlen) {
  out[0] = '\0';
#ifdef __APPLE__
  FILE *fp = popen("sw_vers -productName 2>/dev/null", "r");
  if (fp) {
    char buf[64];
    if (fgets(buf, sizeof(buf), fp)) {
      trim_row(buf);
      int len = (int)strlen(buf);
      if (len > 0 && len < maxlen) {
        memcpy(out, buf, len + 1);
        pclose(fp);
        return 1;
      }
    }
    pclose(fp);
  }
  strncpy(out, "macos", maxlen - 1);
  out[maxlen - 1] = '\0';
  return 1;
#else
  FILE *fp = fopen("/etc/os-release", "r");
  if (!fp)
    return 0;
  char buf[256];
  int found = 0;
  while (fgets(buf, sizeof(buf), fp)) {
    if (strncmp(buf, "ID=", 3) == 0)
      found = parse_os_release_val(buf, 3, out, maxlen);
    else if (strncmp(buf, "ID_LIKE=", 8) == 0)
      parse_os_release_val(buf, 8, g_distro_id_like, sizeof(g_distro_id_like));
  }
  fclose(fp);
  return found;
#endif
}

/* ---------------------------------------------------------- built-in art */
/* The last-resort fallback: a plain Tux from the embedded set if present,
 * otherwise a blocky "N". */
void logo_load_default(void) {
  if (logo_load_builtin("linux"))
    return;

  static const char *art[] = {
      "  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"
      "                  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
      "  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"
      "                \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
      "  \xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"
      "              \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
      "  \xe2\x96\x88\xe2\x96\x88    \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"
      "            \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
      "  \xe2\x96\x88\xe2\x96\x88      \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"
      "          \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
      "  \xe2\x96\x88\xe2\x96\x88        \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"
      "        \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
      "  \xe2\x96\x88\xe2\x96\x88          \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"
      "      \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
      "  \xe2\x96\x88\xe2\x96\x88            \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"
      "    \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
      "  \xe2\x96\x88\xe2\x96\x88              \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"
      "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
      "  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"
      "          \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
      "  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"
      "            \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
  };
  logo_rows = (int)(sizeof(art) / sizeof(art[0]));
  for (int i = 0; i < logo_rows; i++) {
    int len = (int)strlen(art[i]);
    memcpy(logo_data[i], art[i], len + 1);
  }
}

/* ----------------------------------------------------------- distro tint */
/* Plain (non-ANSI) logos are tinted with the distribution's canonical colour;
 * unknown distributions keep the default magenta/white two-tone. */
void logo_set_distro_colors(const char *distro) {
  const char *color = distro ? logo_distro_color(distro) : NULL;
  if (color) {
    static char tint[16];
    snprintf(tint, sizeof(tint), "\033[1;%sm", color);
    g_color_inner = tint;
    g_color_outer = tint;
    return;
  }
  if (distro && strcasecmp(distro, "macos") == 0) {
    g_color_outer = "\033[1;36m";
    g_color_inner = "\033[1;37m";
    return;
  }
  /* Default (also gentoo): magenta sides, white faces. */
  g_color_outer = "\033[1;35m";
  g_color_inner = "\033[1;37m";
}
