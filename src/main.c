/*
 * main.c - command line handling and orchestration.
 *
 * Order of operations: parse flags, resolve a logo, gather system info, then
 * hand everything to the renderer.
 */
#define _GNU_SOURCE
#include "nexus.h"

#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

struct nexus_options g_opts;

/* A --logo argument made only of name characters (no path separator or
 * extension) is taken as a distro/logo name rather than a file. */
static int looks_like_logo_name(const char *s) {
  if (!s || !*s)
    return 0;
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_' ||
        c == '+')
      continue;
    return 0;
  }
  return 1;
}

static void print_help(void) {
  printf(
      "Usage: nexus [options]\n\n"
      "Options:\n"
      "  -l, --logo <name|path>    Load a logo: a file path, or a distro/\n"
      "                            logo name such as 'arch' or 'ubuntu'\n"
      "  --rotate-x                Lock rotation to the X axis only\n"
      "  --rotate-y                Lock rotation to the Y axis only\n"
      "  -s, --speed <float>       Speed multiplier (default 1.0)\n"
      "  --size <float>            Scale the logo (e.g. 2.0 for double size)\n"
      "  --depth <float>           Scale the 3D depth (default 1.0)\n"
      "  --height <n>              Override render height in rows\n"
      "  --no-info                 Just the logo, no system info\n"
      "  --no-color                Disable logo coloring\n"
      "  --frames <n>              Stop after n frames (default: run until a key)\n"
      "  --shading-chars <str>     Custom shading ramp, supports UTF-8\n"
      "                            (default: .,-~:;=!*#$@)\n"
      "  -V, --version             Show version\n"
      "  -h, --help                Show this help\n\n"
      "Logo:\n"
      "  --logo <name> shows a distro logo, e.g. '--logo arch'\n"
      "  (fastfetch's library when installed, else the embedded set).\n"
      "  --logo <path> loads a logo file; an optional first line\n"
      "  '# distro: <name>' sets the color scheme.\n"
      "  Without --logo, ~/.config/nexus/logo.txt is used if present,\n"
      "  otherwise the detected distro logo.\n");
}

static void print_version(void) {
  struct utsname u;
  const char *arch = "unknown";
  const char *os = "unknown";
  if (uname(&u) == 0) {
    arch = u.machine;
    os = u.sysname;
  }
  printf("nexus %s \"%s\" (%s, %s)\n", NEXUS_VERSION, NEXUS_CODENAME, arch, os);
}

int main(int argc, char **argv) {
  const char *logo_path = NULL;
  int rotate_x = 1, rotate_y = 1;
  float speed = 1.0f;
  int show_info = 1;
  int use_color = 1;
  int max_frames = 0;
  const char *shading = NULL;

  g_opts.height = 0; /* 0 = auto (fit the info block) */
  g_opts.size = 1.0f;
  g_opts.depth = 1.0f;
  g_opts.depth_user_set = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      print_help();
      return 0;
    } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-V") == 0) {
      print_version();
      return 0;
    } else if (strcmp(argv[i], "--logo") == 0 || strcmp(argv[i], "-l") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "nexus: option '%s' requires an argument\n", argv[i]);
        return 1;
      }
      logo_path = argv[++i];
    } else if (strcmp(argv[i], "--rotate-x") == 0) {
      rotate_x = 1;
      rotate_y = 0;
    } else if (strcmp(argv[i], "--rotate-y") == 0) {
      rotate_x = 0;
      rotate_y = 1;
    } else if (strcmp(argv[i], "--speed") == 0 || strcmp(argv[i], "-s") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "nexus: option '%s' requires an argument\n", argv[i]);
        return 1;
      }
      const char *option = argv[i];
      if (!nx_parse_number(option, argv[++i], -1000.0f, 1000.0f, &speed))
        return 1;
    } else if (strcmp(argv[i], "--no-info") == 0) {
      show_info = 0;
    } else if (strcmp(argv[i], "--no-color") == 0) {
      use_color = 0;
    } else if (strcmp(argv[i], "--frames") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "nexus: option '%s' requires an argument\n", argv[i]);
        return 1;
      }
      const char *option = argv[i];
      if (!nx_parse_integer(option, argv[++i], 0, 0x7fffffff, &max_frames))
        return 1;
    } else if (strcmp(argv[i], "--shading-chars") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "nexus: option '%s' requires an argument\n", argv[i]);
        return 1;
      }
      shading = argv[++i];
    } else if (strcmp(argv[i], "--height") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "nexus: option '%s' requires an argument\n", argv[i]);
        return 1;
      }
      const char *option = argv[i];
      if (!nx_parse_integer(option, argv[++i], 1, NEXUS_MAX_HEIGHT, &g_opts.height))
        return 1;
    } else if (strcmp(argv[i], "--size") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "nexus: option '%s' requires an argument\n", argv[i]);
        return 1;
      }
      const char *option = argv[i];
      if (!nx_parse_number(option, argv[++i], 0.5f, 5.0f, &g_opts.size))
        return 1;
    } else if (strcmp(argv[i], "--depth") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "nexus: option '%s' requires an argument\n", argv[i]);
        return 1;
      }
      const char *option = argv[i];
      if (!nx_parse_number(option, argv[++i], 0.1f, 10.0f, &g_opts.depth))
        return 1;
      g_opts.depth_user_set = 1;
    } else {
      fprintf(stderr, "nexus: unknown option: %s\nTry 'nexus --help'\n",
              argv[i]);
      return 1;
    }
  }

  render_set_shading(shading);

  /* ------------------------------------------------------------- logo */
  int have_logo = 0;
  int logo_named = 0; /* --logo was a distro name, not a file */
  if (logo_path) {
    have_logo = logo_load_path(logo_path);
    if (!have_logo && looks_like_logo_name(logo_path)) {
      /* Not a file: treat the argument as a distro/logo name. */
      have_logo = logo_load_fastfetch(logo_path);
      if (!have_logo)
        have_logo = logo_load_builtin(logo_path);
      if (have_logo)
        logo_named = 1;
    }
    if (!have_logo)
      fprintf(stderr, "nexus: couldn't read logo '%s', using the detected logo\n",
              logo_path);
  } else {
    have_logo = logo_load_file();
  }

  /* An explicit --logo name wins, then a logo file's "# distro:" line, then
   * /etc/os-release. The name selects both the logo and its colour. */
  char distro[64] = "";
  if (logo_named) {
    strncpy(distro, logo_path, sizeof(distro) - 1);
    distro[sizeof(distro) - 1] = '\0';
  } else if (g_file_distro[0]) {
    strncpy(distro, g_file_distro, sizeof(distro) - 1);
    distro[sizeof(distro) - 1] = '\0';
  } else {
    logo_detect_distro(distro, sizeof(distro));
  }

  /* Without a file logo, borrow fastfetch's library (when installed), then the
   * embedded core set, then a plain Tux and finally the blocky "N". */
  if (!have_logo)
    have_logo = logo_load_fastfetch(distro[0] ? distro : NULL);

  if (!have_logo && distro[0])
    have_logo = logo_load_builtin(distro);

  if (!have_logo && g_distro_id_like[0]) {
    /* ID_LIKE is a space-separated list of parent distributions. */
    char likes[64];
    strncpy(likes, g_distro_id_like, sizeof(likes) - 1);
    likes[sizeof(likes) - 1] = '\0';
    for (char *tok = strtok(likes, " \t"); tok && !have_logo;
         tok = strtok(NULL, " \t"))
      have_logo = logo_load_builtin(tok);
  }

  if (!have_logo)
    logo_load_default();

  logo_process();
  if (distro[0])
    logo_set_distro_colors(distro);

  /* ------------------------------------------------------------- info */
  if (show_info)
    info_gather_all();

  render_run(show_info, use_color, max_frames, rotate_x, rotate_y, speed);
  return 0;
}
