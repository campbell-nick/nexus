/*
 * render.c - the 3D engine, layout and animation loop.
 *
 * The pipeline: height map -> extruded point cloud -> rotated + perspective
 * projected -> shaded with Blinn-Phong -> collapsed to ASCII glyphs and
 * written out in one syscall per frame.
 */
#define _GNU_SOURCE
#include "nexus.h"

#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* --------------------------------------------------------------- layout */
int render_height = 36;
int logo_height = 36;
int anim_width = NEXUS_ANIM_WIDTH;
int layout_stacked = 0;
int info_clip_cols = -1;
int stacked_info_rows = 0;
int term_rows = 0;
int term_cols = 0;

static volatile sig_atomic_t term_resized = 0;

static void cleanup(void) {
  printf("\033[?25h");
  fflush(stdout);
}

static void handle_signal(int sig) {
  (void)sig;
  cleanup();
  _exit(0);
}

static void handle_winch(int sig) {
  (void)sig;
  term_resized = 1;
}

static void get_term_size(int *rows, int *cols) {
  struct winsize ws;
  *rows = 0;
  *cols = 0;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
    if (ws.ws_row > 0)
      *rows = ws.ws_row;
    if (ws.ws_col > 0)
      *cols = ws.ws_col;
  }
}

/* -------------------------------------------------------------- shading */
#define NEXUS_RAMP_ASCII ".,-~:;=!*#$@"
#define NEXUS_SHADING_MAX 64

static char shading_chars[NEXUS_SHADING_MAX][5];
static int shading_count = 0;

static void parse_shading(const char *str) {
  shading_count = 0;
  const char *p = str;
  while (*p && shading_count < NEXUS_SHADING_MAX) {
    int len = nx_utf8_len((unsigned char)*p);
    if (len > 4)
      len = 4;
    int actual = 0;
    while (actual < len && p[actual])
      actual++;
    memcpy(shading_chars[shading_count], p, actual);
    shading_chars[shading_count][actual] = '\0';
    shading_count++;
    p += actual;
  }
  if (shading_count == 0) {
    strcpy(shading_chars[0], ".");
    shading_count = 1;
  }
}

void render_set_shading(const char *chars) {
  parse_shading((chars && chars[0]) ? chars : NEXUS_RAMP_ASCII);
}

/* --------------------------------------------------------------- buffers */
static float zbuf[NEXUS_MAX_HEIGHT][NEXUS_ANIM_WIDTH];
static float lumbuf[NEXUS_MAX_HEIGHT][NEXUS_ANIM_WIDTH];
static int colorbuf[NEXUS_MAX_HEIGHT][NEXUS_ANIM_WIDTH];

static void clear_buf(void) {
  size_t n = (size_t)render_height * NEXUS_ANIM_WIDTH;
  memset(zbuf, 0, n * sizeof(float));
  memset(lumbuf, 0, n * sizeof(float));
  memset(colorbuf, 0, n * sizeof(int));
}

/* Pick the glyph for a cell from its luminance. Returns NULL for empty cells. */
static const char *cell_glyph(int row, int col, int smax, int *color_out) {
  if (zbuf[row][col] <= 0.0f)
    return NULL;
  *color_out = colorbuf[row][col];
  int ci = (int)(lumbuf[row][col] * smax + 0.5f);
  if (ci < 0)
    ci = 0;
  if (ci > smax)
    ci = smax;
  return shading_chars[ci];
}

/* ---------------------------------------------------------------- layout */
static void render_apply_layout(int show_info) {
  int rows = term_rows;
  int cols = term_cols;
  if (rows > 1)
    rows--; /* leave a margin to avoid scroll jitter */

  int ideal = (int)(36 * g_opts.size);
  if (g_opts.height > 0)
    ideal = g_opts.height;
  else if (show_info && info_line_count > 0) {
    int info_height = info_line_count + 2;
    if (ideal < info_height)
      ideal = info_height;
  }
  if (ideal < 20)
    ideal = 20;
  if (ideal > NEXUS_MAX_HEIGHT)
    ideal = NEXUS_MAX_HEIGHT;

  render_height = ideal;
  anim_width = NEXUS_ANIM_WIDTH;
  layout_stacked = 0;
  info_clip_cols = -1;
  stacked_info_rows = 0;

  if (cols <= 0) {
    if (rows > 0 && render_height > rows)
      render_height = rows;
    logo_height = render_height;
    return;
  }

  int info_cols = 0;
  if (show_info)
    for (int i = 0; i < info_line_count; i++) {
      int w = nx_visible_width(info_lines[i]);
      if (w > info_cols)
        info_cols = w;
    }

  if (show_info && cols < NEXUS_ANIM_WIDTH + NEXUS_GAP + info_cols) {
    anim_width = cols - NEXUS_GAP - info_cols;
    if (anim_width < 20) {
      layout_stacked = 1;
      anim_width = cols < NEXUS_ANIM_WIDTH ? cols : NEXUS_ANIM_WIDTH;
    }
  } else if (anim_width > cols) {
    anim_width = cols;
  }

  int wcap = anim_width * 3 / 5;

  if (layout_stacked) {
    if (render_height > wcap)
      render_height = wcap;
    stacked_info_rows = info_line_count;
    if (rows > 0) {
      int budget = rows - info_line_count - 1;
      if (budget < render_height)
        render_height = budget;
      if (render_height < 6)
        render_height = 0;
      int info_budget = rows - (render_height ? render_height + 1 : 0);
      if (stacked_info_rows > info_budget)
        stacked_info_rows = info_budget < 0 ? 0 : info_budget;
    }
    logo_height = render_height;
    info_clip_cols = cols;
  } else {
    if (show_info)
      info_clip_cols = cols - anim_width - NEXUS_GAP;
    if (rows > 0 && render_height > rows)
      render_height = rows;
    logo_height = render_height > wcap ? wcap : render_height;
    if (!show_info)
      render_height = logo_height;
  }
}

/* ----------------------------------------------------------- point cloud */
#define MAX_POINTS NEXUS_MAX_POINTS
static float PX[MAX_POINTS], PY[MAX_POINTS], PZ[MAX_POINTS];
static float PNX[MAX_POINTS], PNY[MAX_POINTS], PNZ[MAX_POINTS];
static int PCOLOR[MAX_POINTS];
static int POINT_COUNT = 0;

static void render_build_points(void) {
  const float sx = 0.07f;
  const float sy = 0.14f;
  const float cx = (logo_cols - 1) * 0.5f;
  const float cy = (logo_rows - 1) * 0.5f;
  int z_layers = (int)(6 * g_opts.size);
  if (z_layers < 6)
    z_layers = 6;

  float(*hmap)[NEXUS_MAX_LOGO_COLS] =
      malloc(sizeof(float[NEXUS_MAX_LOGO_ROWS][NEXUS_MAX_LOGO_COLS]));
  float(*gnx)[NEXUS_MAX_LOGO_COLS] =
      malloc(sizeof(float[NEXUS_MAX_LOGO_ROWS][NEXUS_MAX_LOGO_COLS]));
  float(*gny)[NEXUS_MAX_LOGO_COLS] =
      malloc(sizeof(float[NEXUS_MAX_LOGO_ROWS][NEXUS_MAX_LOGO_COLS]));
  float(*gnz)[NEXUS_MAX_LOGO_COLS] =
      malloc(sizeof(float[NEXUS_MAX_LOGO_ROWS][NEXUS_MAX_LOGO_COLS]));
  if (!hmap || !gnx || !gny || !gnz) {
    free(hmap);
    free(gnx);
    free(gny);
    free(gnz);
    POINT_COUNT = 0;
    return;
  }

  for (int r = 0; r < logo_rows; r++)
    for (int c = 0; c < logo_cols; c++)
      hmap[r][c] = (c < logo_cell_counts[r]) ? logo_char_weight(logo_cells[r][c])
                                             : 0.0f;

  /* Flat logos (low height variance) look paper-thin, so auto-boost depth. */
  if (!g_opts.depth_user_set) {
    float sum = 0, sum2 = 0;
    int n = 0;
    for (int r = 0; r < logo_rows; r++)
      for (int c = 0; c < logo_cols; c++)
        if (hmap[r][c] > 0.0f) {
          sum += hmap[r][c];
          sum2 += hmap[r][c] * hmap[r][c];
          n++;
        }
    if (n > 0) {
      float mean = sum / n;
      float variance = sum2 / n - mean * mean;
      float stddev = sqrtf(variance > 0 ? variance : 0);
      if (stddev < 0.25f) {
        float boost = 1.0f + 2.0f * (0.25f - stddev) / 0.25f;
        g_opts.depth *= boost;
      }
    }
  }

  const float zmax = 0.18f * g_opts.depth;

  for (int r = 0; r < logo_rows; r++) {
    for (int c = 0; c < logo_cols; c++) {
      if (hmap[r][c] <= 0.0f) {
        gnx[r][c] = gny[r][c] = 0;
        gnz[r][c] = 1;
        continue;
      }
      float dhdx = 0, dhdy = 0;
      if (c > 0 && c < logo_cols - 1)
        dhdx = (hmap[r][c + 1] - hmap[r][c - 1]) * 0.5f;
      else if (c == 0 && logo_cols > 1)
        dhdx = hmap[r][c + 1] - hmap[r][c];
      else if (c > 0)
        dhdx = hmap[r][c] - hmap[r][c - 1];

      if (r > 0 && r < logo_rows - 1)
        dhdy = (hmap[r + 1][c] - hmap[r - 1][c]) * 0.5f;
      else if (r == 0 && logo_rows > 1)
        dhdy = hmap[r + 1][c] - hmap[r][c];
      else if (r > 0)
        dhdy = hmap[r][c] - hmap[r - 1][c];

      dhdx /= sx;
      dhdy /= sy;

      float nnx = -dhdx, nny = dhdy, nnz = 1.0f;
      float l = sqrtf(nnx * nnx + nny * nny + nnz * nnz);
      gnx[r][c] = nnx / l;
      gny[r][c] = nny / l;
      gnz[r][c] = nnz / l;
    }
  }

  int subdiv = (int)g_opts.size;
  if (subdiv < 1)
    subdiv = 1;

  int idx = 0;
  for (int row = 0; row < logo_rows; row++) {
    for (int col = 0; col < logo_cols; col++) {
      float h = hmap[row][col];
      if (h <= 0.0f)
        continue;

      for (int sr = 0; sr < subdiv; sr++) {
        for (int sc = 0; sc < subdiv; sc++) {
          float frow = row + (float)sr / subdiv;
          float fcol = col + (float)sc / subdiv;

          float ih = h;
          if (sr > 0 || sc > 0) {
            float fr = (float)sr / subdiv;
            float fc = (float)sc / subdiv;
            int nr = row + (sr > 0 ? 1 : 0);
            int nc = col + (sc > 0 ? 1 : 0);
            if (nr >= logo_rows)
              nr = logo_rows - 1;
            if (nc >= logo_cols)
              nc = logo_cols - 1;
            float h00 = hmap[row][col];
            float h10 = hmap[nr][col];
            float h01 = hmap[row][nc];
            float h11 = hmap[nr][nc];
            ih = h00 * (1 - fr) * (1 - fc) + h10 * fr * (1 - fc) +
                 h01 * (1 - fr) * fc + h11 * fr * fc;
            if (ih <= 0.0f)
              continue;
          }

          float ox = (fcol - cx) * sx;
          float oy = (cy - frow) * sy;
          float zr = ih * zmax;

          /* Edge cells only get front/back faces; interior cells are solid. */
          int is_edge = 0;
          for (int dr = -1; dr <= 1 && !is_edge; dr++)
            for (int dc = -1; dc <= 1 && !is_edge; dc++) {
              if (dr == 0 && dc == 0)
                continue;
              int nr = row + dr, nc = col + dc;
              float nh = 0;
              if (nr >= 0 && nr < logo_rows && nc >= 0 && nc < logo_cols)
                nh = hmap[nr][nc];
              if (nh <= 0.0f)
                is_edge = 1;
            }
          int layers = (is_edge || ih < 0.15f) ? 2 : z_layers;

          for (int k = 0; k < layers; k++) {
            if (idx >= MAX_POINTS)
              break;
            float t = ((float)k / (layers - 1)) - 0.5f;
            PX[idx] = ox;
            PY[idx] = oy;
            PZ[idx] = t * 2.0f * zr;
            PCOLOR[idx] = logo_has_ansi ? logo_cell_color[row][col]
                                        : (k == 0 || k == layers - 1);

            if (k == 0) {
              PNX[idx] = gnx[row][col];
              PNY[idx] = gny[row][col];
              PNZ[idx] = -gnz[row][col];
            } else if (k == layers - 1) {
              PNX[idx] = gnx[row][col];
              PNY[idx] = gny[row][col];
              PNZ[idx] = gnz[row][col];
            } else {
              float ex = 0, ey = 0;
              for (int dr = -1; dr <= 1; dr++)
                for (int dc = -1; dc <= 1; dc++) {
                  if (dr == 0 && dc == 0)
                    continue;
                  int nr = row + dr, nc = col + dc;
                  float nh = 0;
                  if (nr >= 0 && nr < logo_rows && nc >= 0 && nc < logo_cols)
                    nh = hmap[nr][nc];
                  if (nh < h) {
                    ex += (float)dc;
                    ey += (float)(-dr);
                  }
                }
              float el = sqrtf(ex * ex + ey * ey);
              if (el > 1e-6f) {
                ex /= el;
                ey /= el;
              }
              float tn = ((float)k / (layers - 1)) * 2.0f - 1.0f;
              float side = sqrtf(1.0f - tn * tn);
              PNX[idx] = ex * side;
              PNY[idx] = ey * side;
              PNZ[idx] = tn;
            }
            idx++;
          }
        }
      }
    }
  }
  POINT_COUNT = idx;
  free(hmap);
  free(gnx);
  free(gny);
  free(gnz);
}

/* --------------------------------------------------------------- the loop */
void render_run(int show_info, int use_color, int max_frames, int rotate_x,
                int rotate_y, float speed) {
  get_term_size(&term_rows, &term_cols);

  render_apply_layout(show_info);
  render_build_points();

  float A = 0.0f, B = 0.0f;
  float K1 = 37.0f * logo_height / 36.0f * g_opts.size;
  const float K2 = 5.5f;

  /* Face-on extent fixes the vertical framing so the top lands at row 1. */
  float face_up = 0, face_dn = 0;
  for (int i = 0; i < POINT_COUNT; i++) {
    float zc = PZ[i] + K2;
    if (zc < 0.1f)
      continue;
    float ys = K1 * PY[i] / zc;
    if (ys > face_up)
      face_up = ys;
    if (-ys > face_dn)
      face_dn = -ys;
  }
  float fixed_y_center = face_up + 1.0f;

  const float lx = 0.4082f, ly = 0.8165f, lz = -0.4082f; /* fixed top-left */
  const float hx0 = lx, hy0 = ly, hz0 = lz - 1.0f;
  const float hl0 = sqrtf(hx0 * hx0 + hy0 * hy0 + hz0 * hz0);
  const float hlx = hx0 / hl0, hly = hy0 / hl0, hlz = hz0 / hl0;

  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);
  signal(SIGWINCH, handle_winch);
  atexit(cleanup);

  int fetch_start = show_info ? 1 : 0;
  if (g_opts.height == 0) {
    int logo_bottom = (int)(fixed_y_center + face_dn) + 2;
    int info_bottom = show_info ? fetch_start + info_line_count + 1 : 0;
    int needed = logo_bottom > info_bottom ? logo_bottom : info_bottom;
    if (needed < render_height)
      render_height = needed;
  }

  printf("\033[?25l\033[2J");
  fflush(stdout);

  const int smax = shading_count - 1;

  for (int frame = 0; max_frames == 0 || frame < max_frames; frame++) {
    /* Any pending input: leave the keypress for the shell and stop. */
    struct pollfd pfd = {.fd = STDIN_FILENO, .events = POLLIN};
    if (poll(&pfd, 1, 0) > 0)
      break;

    if (term_resized) {
      term_resized = 0;
      get_term_size(&term_rows, &term_cols);
      int old_h = render_height, old_w = anim_width;
      int old_stacked = layout_stacked, old_clip = info_clip_cols;
      render_apply_layout(show_info);
      if (render_height != old_h || anim_width != old_w ||
          layout_stacked != old_stacked || info_clip_cols != old_clip) {
        K1 = 37.0f * logo_height / 36.0f * g_opts.size;
        printf("\033[2J");
        fflush(stdout);
      }
    }

    clear_buf();
    A += rotate_x ? 0.04f * speed : 0.0f;
    B += rotate_y ? 0.06f * speed : 0.0f;
    float cA = cosf(A), sA = sinf(A);
    float cB = cosf(B), sB = sinf(B);

    const float y_center =
        (!layout_stacked && show_info) ? fixed_y_center : render_height * 0.5f;
    const float half_aw = (float)anim_width * 0.5f;
    const float k1x2 = K1 * 2.0f;
    const int aw = anim_width;

    for (int i = 0; i < POINT_COUNT; i++) {
      float px = PX[i], py = PY[i], pz = PZ[i];
      float nx = PNX[i], ny = PNY[i], nz = PNZ[i];

      float y1 = py * cA - pz * sA;
      float z1 = py * sA + pz * cA;
      float x2 = px * cB + z1 * sB;
      float z2 = -px * sB + z1 * cB;
      float y2 = y1;

      float ny1 = ny * cA - nz * sA;
      float nz1 = ny * sA + nz * cA;
      float nx2 = nx * cB + nz1 * sB;
      float nz2 = -nx * sB + nz1 * cB;
      float ny2 = ny1;

      float zc = z2 + K2;
      if (zc < 0.1f)
        continue;
      float ooz = 1.0f / zc;
      int xs = (int)(half_aw + k1x2 * x2 * ooz);
      int ys = (int)(y_center - K1 * y2 * ooz);
      if (xs < 0 || xs >= aw || ys < 0 || ys >= render_height)
        continue;

      if (ooz > zbuf[ys][xs]) {
        float diff = nx2 * lx + ny2 * ly + nz2 * lz;
        if (diff < 0)
          diff = 0;
        float spec_dot = nx2 * hlx + ny2 * hly + nz2 * hlz;
        if (spec_dot < 0)
          spec_dot = 0;
        float spec = spec_dot * spec_dot;
        spec = spec * spec;
        spec = spec * spec;
        float L = 0.08f + 0.62f * diff + 0.30f * spec;
        if (L > 1.0f)
          L = 1.0f;

        zbuf[ys][xs] = ooz;
        lumbuf[ys][xs] = L;
        colorbuf[ys][xs] = PCOLOR[i];
      }
    }

    static char *out_buf = NULL;
    static size_t out_cap = 0;
    size_t need = (size_t)(render_height + stacked_info_rows + 2) * 2048u + 64u;
    if (need > out_cap) {
      free(out_buf);
      out_buf = malloc(need);
      out_cap = out_buf ? need : 0;
    }
    if (!out_buf) {
      printf("\033[?25h");
      return;
    }
    char *p = out_buf;
    char *end = out_buf + need - 8;
    *p++ = '\033';
    *p++ = '[';
    *p++ = 'H';

    const char reset_seq[] = "\033[0m";
    const char clr_seq[] = "\033[K";

    static char ansi_tbl[128][8];
    static int ansi_len[128];
    int inner_len = (int)strlen(g_color_inner);
    int outer_len = (int)strlen(g_color_outer);

    for (int i = 0; i < render_height && p + 8 < end; i++) {
      if (!use_color) {
        for (int j = 0; j < aw && p + 8 < end; j++) {
          int c = 0;
          const char *sc = cell_glyph(i, j, smax, &c);
          if (!sc) {
            *p++ = ' ';
            continue;
          }
          int k = 0;
          while (k < 4 && sc[k]) {
            p[k] = sc[k];
            k++;
          }
          p += k ? k : 1;
        }
      } else {
        int prev_color = -1;
        for (int j = 0; j < aw && p + 16 < end; j++) {
          int c = 0;
          const char *sc = cell_glyph(i, j, smax, &c);
          if (!sc) {
            if (prev_color != -1) {
              memcpy(p, reset_seq, 4);
              p += 4;
              prev_color = -1;
            }
            *p++ = ' ';
          } else {
            if (c != prev_color) {
              if (logo_has_ansi && c > 0 && c < 128) {
                if (!ansi_len[c])
                  ansi_len[c] = snprintf(ansi_tbl[c], sizeof(ansi_tbl[c]),
                                         "\033[1;%dm", c);
                memcpy(p, ansi_tbl[c], ansi_len[c]);
                p += ansi_len[c];
              } else {
                const char *cs = (c == 1) ? g_color_inner : g_color_outer;
                int clen = (c == 1) ? inner_len : outer_len;
                memcpy(p, cs, clen);
                p += clen;
              }
              prev_color = c;
            }
            int k = 0;
            while (k < 4 && sc[k]) {
              p[k] = sc[k];
              k++;
            }
            p += k ? k : 1;
          }
        }
        if (prev_color != -1 && p + 4 < end) {
          memcpy(p, reset_seq, 4);
          p += 4;
        }
      }

      int fi = i - fetch_start;
      if (!layout_stacked && fi >= 0 && fi < info_line_count &&
          p + NEXUS_GAP + 4 < end) {
        memset(p, ' ', NEXUS_GAP);
        p += NEXUS_GAP;
        p = nx_emit_clipped(p, end, info_lines[fi], info_clip_cols);
      }

      if (p + 8 >= end)
        break;
      memcpy(p, clr_seq, 3);
      p += 3;
      *p++ = '\n';
    }

    if (layout_stacked && stacked_info_rows > 0) {
      if (render_height > 0 && p + 8 < end) {
        memcpy(p, clr_seq, 3);
        p += 3;
        *p++ = '\n';
      }
      for (int i = 0; i < stacked_info_rows && p + 16 < end; i++) {
        p = nx_emit_clipped(p, end, info_lines[i], info_clip_cols);
        memcpy(p, clr_seq, 3);
        p += 3;
        *p++ = '\n';
      }
    }
    if (write(STDOUT_FILENO, out_buf, (size_t)(p - out_buf)) < 0)
      break;
    usleep(50000);
  }

  printf("\033[?25h");
  fflush(stdout);
}
