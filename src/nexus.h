/*
 * nexus - a spinning 3D terminal fetch tool.
 *
 * Shared declarations for the whole program. The renderer takes an ASCII /
 * Unicode logo, lifts each glyph into a height map, extrudes it into a point
 * cloud and shades it with Blinn-Phong lighting while system information is
 * shown alongside it.
 */
#ifndef NEXUS_H
#define NEXUS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define NEXUS_VERSION "1.6.0"
#define NEXUS_CODENAME "Prism"

#define NEXUS_ANIM_WIDTH 60
#define NEXUS_MAX_HEIGHT 200
#define NEXUS_GAP 2

#define NEXUS_MAX_POINTS 200000
#define NEXUS_MAX_LOGO_ROWS 64
#define NEXUS_MAX_LOGO_COLS 128
#define NEXUS_MAX_LINES 48
#define NEXUS_MAX_LINE_LEN 512
#define NEXUS_MAX_CUSTOM 16

/* Render settings, all overridable from the command line. */
struct nexus_options {
  int height; /* 0 = auto (fit the info block) */
  float size;
  float depth;
  int depth_user_set;
};
extern struct nexus_options g_opts;

/* ---------------------------------------------------------------- util.c */
int nx_utf8_len(unsigned char c);
int nx_skip_ansi(const char *p);
int nx_visible_width(const char *s);
char *nx_emit_clipped(char *p, char *end, const char *s, int max_cols);
int nx_parse_number(const char *option, const char *text, float min,
                    float max, float *out);
int nx_parse_integer(const char *option, const char *text, int min, int max,
                     int *out);
int nx_read_first_line(const char *path, char *out, int outlen);

/* ---------------------------------------------------------------- logo.c */
extern char logo_cells[NEXUS_MAX_LOGO_ROWS][NEXUS_MAX_LOGO_COLS][5];
extern int logo_cell_color[NEXUS_MAX_LOGO_ROWS][NEXUS_MAX_LOGO_COLS];
extern int logo_cell_counts[NEXUS_MAX_LOGO_ROWS];
extern int logo_rows;
extern int logo_cols;
extern int logo_has_ansi;
extern char g_file_distro[64];
extern char g_distro_id_like[64];
extern const char *g_color_inner;
extern const char *g_color_outer;

float logo_char_weight(const char *ch);
void logo_process(void);
int logo_load_path(const char *path);
int logo_load_file(void);
int logo_load_fastfetch(const char *name);
int logo_load_builtin(const char *id);
void logo_load_default(void);
int logo_detect_distro(char *out, int maxlen);
void logo_set_distro_colors(const char *distro);

/* ------------------------------------------------------------- distros.c */
/* Embedded plain-text logos used when fastfetch is unavailable. */
const char *const *logo_builtin_find(const char *id, int *rows);
const char *logo_distro_color(const char *id);

/* ---------------------------------------------------------------- info.c */
extern char info_lines[NEXUS_MAX_LINES][NEXUS_MAX_LINE_LEN];
extern int info_line_count;

/* Style the label/value pair before gathering. Pass NULL or "" to keep the
 * built-in default for that part (separator ":", key colour "1;35", values
 * uncoloured). */
void info_configure(const char *separator, const char *key_color,
                    const char *value_color);
/* Title format: NULL or "" keeps the built-in "user@host"; "none" hides the
 * title; anything else is used verbatim with {user} and {hostname} replaced. */
void info_set_title(const char *format);
/* Supply custom info lines; each may contain {field} placeholders. */
void info_set_custom(const char *const *lines, int count);
/* Gather every field, then lay the info block out in the requested order.
 * `fields` is a comma/space list of field names (title, os, host, kernel,
 * uptime, age, packages, shell, cpu, gpu, memory, swap, disk, custom, palette);
 * NULL or "" uses the built-in order. `show_palette` appends the colour
 * palette unless it was placed explicitly. */
void info_gather_all(const char *fields, int show_palette);

/* -------------------------------------------------------------- config.c */
/* Create ~/.config/nexus/nexus.conf with a template if it does not exist. */
void config_ensure(void);
/* Look up a key in the config file, copying its value into out. Returns 1 when
 * the key is present, 0 when the file or the key is missing. */
int config_get(const char *key, char *out, size_t outlen);
/* Use a specific config file (from --config or --profile) instead of the
 * default ~/.config/nexus/nexus.conf. */
void config_set_path(const char *path);
/* Collect every value for a repeated key into out (up to max). Returns the
 * number of values found. */
int config_get_all(const char *key, char (*out)[NEXUS_MAX_LINE_LEN], int max);

/* -------------------------------------------------------------- render.c */
void render_set_shading(const char *chars);
/* Put the logo on the right of the info block instead of the left. */
void render_set_logo_side(int right);
void render_run(int show_info, int use_color, int max_frames, int rotate_x,
                int rotate_y, float speed, int spin);

#endif /* NEXUS_H */
