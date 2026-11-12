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
#include <strings.h>
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

/* Parse a boolean config value. Accepts true/false, yes/no, on/off and 1/0
 * (case-insensitively); anything else warns and keeps the current value. */
static int config_bool(const char *key, const char *text, int fallback) {
  if (strcasecmp(text, "true") == 0 || strcasecmp(text, "yes") == 0 ||
      strcasecmp(text, "on") == 0 || strcmp(text, "1") == 0)
    return 1;
  if (strcasecmp(text, "false") == 0 || strcasecmp(text, "no") == 0 ||
      strcasecmp(text, "off") == 0 || strcmp(text, "0") == 0)
    return 0;
  fprintf(stderr, "nexus: %s must be true or false (got '%s')\n", key, text);
  return fallback;
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
      "Config:\n"
      "  ~/.config/nexus/nexus.conf is created on first run; one 'key = value'\n"
      "  per line. It mirrors every flag above:\n"
      "    logo, shading_chars, rotation (x|y|both), speed, size, depth,\n"
      "    height, frames, info (true|false), color (true|false)\n"
      "  'default' restores a built-in value; command-line flags always win.\n\n"
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

  /* Which options the command line set explicitly; the config file fills in
   * the rest, so flags always win. */
  int logo_given = 0, rotation_given = 0, speed_given = 0, size_given = 0,
      depth_given = 0, height_given = 0, frames_given = 0, info_given = 0,
      color_given = 0, shading_given = 0;

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
      logo_given = 1;
    } else if (strcmp(argv[i], "--rotate-x") == 0) {
      rotate_x = 1;
      rotate_y = 0;
      rotation_given = 1;
    } else if (strcmp(argv[i], "--rotate-y") == 0) {
      rotate_x = 0;
      rotate_y = 1;
      rotation_given = 1;
    } else if (strcmp(argv[i], "--speed") == 0 || strcmp(argv[i], "-s") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "nexus: option '%s' requires an argument\n", argv[i]);
        return 1;
      }
      const char *option = argv[i];
      if (!nx_parse_number(option, argv[++i], -1000.0f, 1000.0f, &speed))
        return 1;
      speed_given = 1;
    } else if (strcmp(argv[i], "--no-info") == 0) {
      show_info = 0;
      info_given = 1;
    } else if (strcmp(argv[i], "--no-color") == 0) {
      use_color = 0;
      color_given = 1;
    } else if (strcmp(argv[i], "--frames") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "nexus: option '%s' requires an argument\n", argv[i]);
        return 1;
      }
      const char *option = argv[i];
      if (!nx_parse_integer(option, argv[++i], 0, 0x7fffffff, &max_frames))
        return 1;
      frames_given = 1;
    } else if (strcmp(argv[i], "--shading-chars") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "nexus: option '%s' requires an argument\n", argv[i]);
        return 1;
      }
      shading = argv[++i];
      shading_given = 1;
    } else if (strcmp(argv[i], "--height") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "nexus: option '%s' requires an argument\n", argv[i]);
        return 1;
      }
      const char *option = argv[i];
      if (!nx_parse_integer(option, argv[++i], 1, NEXUS_MAX_HEIGHT, &g_opts.height))
        return 1;
      height_given = 1;
    } else if (strcmp(argv[i], "--size") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "nexus: option '%s' requires an argument\n", argv[i]);
        return 1;
      }
      const char *option = argv[i];
      if (!nx_parse_number(option, argv[++i], 0.5f, 5.0f, &g_opts.size))
        return 1;
      size_given = 1;
    } else if (strcmp(argv[i], "--depth") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "nexus: option '%s' requires an argument\n", argv[i]);
        return 1;
      }
      const char *option = argv[i];
      if (!nx_parse_number(option, argv[++i], 0.1f, 10.0f, &g_opts.depth))
        return 1;
      g_opts.depth_user_set = 1;
      depth_given = 1;
    } else {
      fprintf(stderr, "nexus: unknown option: %s\nTry 'nexus --help'\n",
              argv[i]);
      return 1;
    }
  }

  /* ---------------------------------------------------------- config file */
  /* Values from ~/.config/nexus/nexus.conf fill in whatever the command line
   * did not set; flags always win. */
  config_ensure();
  char cfg[256];
  char cfg_logo[512];
  char cfg_shading[256];

  if (!logo_given && config_get("logo", cfg_logo, sizeof(cfg_logo)) && cfg_logo[0])
    logo_path = cfg_logo;

  if (!shading_given && config_get("shading_chars", cfg_shading, sizeof(cfg_shading)) &&
      cfg_shading[0] && strcasecmp(cfg_shading, "default") != 0)
    shading = cfg_shading;

  if (!rotation_given && config_get("rotation", cfg, sizeof(cfg)) && cfg[0]) {
    if (strcasecmp(cfg, "x") == 0) {
      rotate_x = 1;
      rotate_y = 0;
    } else if (strcasecmp(cfg, "y") == 0) {
      rotate_x = 0;
      rotate_y = 1;
    } else if (strcasecmp(cfg, "both") == 0) {
      rotate_x = 1;
      rotate_y = 1;
    } else {
      fprintf(stderr, "nexus: rotation must be x, y or both (got '%s')\n", cfg);
    }
  }

  if (!speed_given && config_get("speed", cfg, sizeof(cfg)) && cfg[0])
    nx_parse_number("speed", cfg, -1000.0f, 1000.0f, &speed);
  if (!size_given && config_get("size", cfg, sizeof(cfg)) && cfg[0])
    nx_parse_number("size", cfg, 0.5f, 5.0f, &g_opts.size);
  if (!depth_given && config_get("depth", cfg, sizeof(cfg)) && cfg[0]) {
    float d;
    if (nx_parse_number("depth", cfg, 0.1f, 10.0f, &d)) {
      g_opts.depth = d;
      /* A value equal to the built-in default leaves the automatic depth
       * boost for flat logos enabled, so the shipped template behaves the
       * same as having no config at all; anything else is an explicit
       * choice and disables the boost. */
      if (d < 0.999f || d > 1.001f)
        g_opts.depth_user_set = 1;
    }
  }
  if (!height_given && config_get("height", cfg, sizeof(cfg)) && cfg[0])
    nx_parse_integer("height", cfg, 0, NEXUS_MAX_HEIGHT, &g_opts.height);
  if (!frames_given && config_get("frames", cfg, sizeof(cfg)) && cfg[0])
    nx_parse_integer("frames", cfg, 0, 0x7fffffff, &max_frames);
  if (!info_given && config_get("info", cfg, sizeof(cfg)) && cfg[0])
    show_info = config_bool("info", cfg, show_info);
  if (!color_given && config_get("color", cfg, sizeof(cfg)) && cfg[0])
    use_color = config_bool("color", cfg, use_color);

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
