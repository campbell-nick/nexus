/*
 * info.c - native system information gathering.
 *
 * Every field is read straight from /proc, /sys or /etc. A few package-manager
 * probes (rpm, xbps, nix) use popen() for accuracy; everything else is a direct
 * file read, so startup stays fast.
 */
#define _GNU_SOURCE
#include "nexus.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#define NEXUS_LABEL "\033[1;35m"

char info_lines[NEXUS_MAX_LINES][NEXUS_MAX_LINE_LEN];
int info_line_count = 0;

/* ------------------------------------------------------------- emission */
static void add_line(const char *line) {
  if (info_line_count >= NEXUS_MAX_LINES)
    return;
  strncpy(info_lines[info_line_count], line, NEXUS_MAX_LINE_LEN - 1);
  info_lines[info_line_count][NEXUS_MAX_LINE_LEN - 1] = '\0';
  info_line_count++;
}

static void info_emit(const char *label, const char *fmt, ...) {
  char val[NEXUS_MAX_LINE_LEN];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(val, sizeof(val), fmt, ap);
  va_end(ap);

  char line[NEXUS_MAX_LINE_LEN];
  snprintf(line, sizeof(line), NEXUS_LABEL "%s\033[0m: %s", label, val);
  add_line(line);
}

/* ----------------------------------------------------------------- title */
static void gather_title(void) {
  char user[64] = "";
  char host[64] = "";
  char *login = getlogin();
  if (login)
    strncpy(user, login, sizeof(user) - 1);
  else {
    char *env = getenv("USER");
    if (env)
      strncpy(user, env, sizeof(user) - 1);
  }
  gethostname(host, sizeof(host));
  host[sizeof(host) - 1] = '\0';

  char line[NEXUS_MAX_LINE_LEN];
  snprintf(line, sizeof(line), NEXUS_LABEL "%s\033[0m@\033[1;35m%s\033[0m", user,
           host);
  add_line(line);

  int title_len = (int)strlen(user) + 1 + (int)strlen(host);
  char sep[NEXUS_MAX_LINE_LEN];
  int pos = 0;
  for (int i = 0; i < title_len && pos + 1 < NEXUS_MAX_LINE_LEN; i++)
    sep[pos++] = '-';
  sep[pos] = '\0';
  add_line(sep);
}

/* ------------------------------------------------------------ small I/O */
static int count_subdirs(const char *path) {
  DIR *d = opendir(path);
  if (!d)
    return 0;
  int count = 0;
  struct dirent *ent;
  while ((ent = readdir(d))) {
    if (ent->d_name[0] == '.')
      continue;
    count++;
  }
  closedir(d);
  return count;
}

static int count_emerge_pkgs(void) {
  DIR *d = opendir("/var/db/pkg");
  if (!d)
    return 0;
  int count = 0;
  struct dirent *cat;
  while ((cat = readdir(d))) {
    if (cat->d_name[0] == '.')
      continue;
    char path[256];
    snprintf(path, sizeof(path), "/var/db/pkg/%s", cat->d_name);
    count += count_subdirs(path);
  }
  closedir(d);
  return count;
}

static int count_file_lines(const char *path, const char *prefix) {
  FILE *fp = fopen(path, "r");
  if (!fp)
    return 0;
  int count = 0;
  char buf[512];
  int plen = prefix ? (int)strlen(prefix) : 0;
  while (fgets(buf, sizeof(buf), fp)) {
    if (!prefix || strncmp(buf, prefix, plen) == 0)
      count++;
  }
  fclose(fp);
  return count;
}

/* --------------------------------------------------------------- gathers */
static void gather_os(void) {
#ifdef __APPLE__
  struct utsname u;
  uname(&u);
  info_emit("OS", "macOS %s", u.machine);
#else
  char pretty[128] = "";
  FILE *fp = fopen("/etc/os-release", "r");
  if (fp) {
    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) {
      if (strncmp(buf, "PRETTY_NAME=", 12) == 0) {
        char *val = buf + 12;
        int len = (int)strlen(val);
        while (len > 0 && (val[len - 1] == '\n' || val[len - 1] == '\r'))
          val[--len] = '\0';
        if (*val == '\'' || *val == '"')
          val++;
        len = (int)strlen(val);
        if (len > 0 && (val[len - 1] == '\'' || val[len - 1] == '"'))
          val[--len] = '\0';
        strncpy(pretty, val, sizeof(pretty) - 1);
        break;
      }
    }
    fclose(fp);
  }
  if (!pretty[0])
    strcpy(pretty, "Linux");
  struct utsname u;
  uname(&u);
  info_emit("OS", "%s %s", pretty, u.machine);
#endif
}

static void gather_host(void) {
#ifndef __APPLE__
  char model[128] = "";
  char vendor[64] = "";
  if (!nx_read_first_line("/proc/device-tree/model", model, sizeof(model))) {
    if (nx_read_first_line("/sys/class/dmi/id/product_name", model,
                           sizeof(model)))
      nx_read_first_line("/sys/class/dmi/id/sys_vendor", vendor, sizeof(vendor));
  }
  if (vendor[0] && strncasecmp(model, vendor, strlen(vendor)) == 0)
    vendor[0] = '\0';
  if (model[0]) {
    if (vendor[0])
      info_emit("Host", "%s %s", vendor, model);
    else
      info_emit("Host", "%s", model);
  }
#endif
}

static void gather_kernel(void) {
  struct utsname u;
  uname(&u);
  info_emit("Kernel", "%s %s", u.sysname, u.release);
}

static void gather_uptime(void) {
#ifdef __APPLE__
  FILE *fp = popen("sysctl -n kern.boottime 2>/dev/null", "r");
  if (!fp)
    return;
  long boot = 0;
  if (fscanf(fp, "sec = %ld", &boot) != 1)
    boot = 0;
  pclose(fp);
  if (boot <= 0)
    return;
  double secs = difftime(time(NULL), (time_t)boot);
#else
  FILE *fp = fopen("/proc/uptime", "r");
  if (!fp)
    return;
  double secs = 0;
  if (fscanf(fp, "%lf", &secs) != 1)
    secs = 0;
  fclose(fp);
#endif

  int total = (int)secs;
  int days = total / 86400;
  int hours = (total % 86400) / 3600;
  int mins = (total % 3600) / 60;

  char val[128];
  if (days > 0)
    snprintf(val, sizeof(val), "%d day%s, %d hour%s, %d min%s", days,
             days == 1 ? "" : "s", hours, hours == 1 ? "" : "s", mins,
             mins == 1 ? "" : "s");
  else if (hours > 0)
    snprintf(val, sizeof(val), "%d hour%s, %d min%s", hours,
             hours == 1 ? "" : "s", mins, mins == 1 ? "" : "s");
  else
    snprintf(val, sizeof(val), "%d min%s", mins, mins == 1 ? "" : "s");
  info_emit("Uptime", "%s", val);
}

/* Birth time (creation time) of the root filesystem, if the kernel and
 * filesystem report one. */
/* statx() is available on glibc >= 2.28 and musl >= 1.2.4. The Makefile probes
 * for it and defines NEXUS_HAVE_STATX=1; when built by hand we fall back to the
 * libc's own STATX_BTIME feature macro. The flags below are part of the stable
 * kernel ABI, so defining any the headers miss keeps older libcs working. */
#ifndef NEXUS_HAVE_STATX
#if defined(STATX_BTIME)
#define NEXUS_HAVE_STATX 1
#else
#define NEXUS_HAVE_STATX 0
#endif
#endif
#if NEXUS_HAVE_STATX
#ifndef STATX_BTIME
#define STATX_BTIME 0x800U
#endif
#ifndef AT_STATX_SYNC_AS_STAT
#define AT_STATX_SYNC_AS_STAT 0x0000
#endif
#endif

static long long root_birth_time(void) {
#ifdef __APPLE__
  struct stat st;
  if (stat("/", &st) == 0 && st.st_birthtimespec.tv_sec > 0)
    return (long long)st.st_birthtimespec.tv_sec;
#elif defined(__linux__) && NEXUS_HAVE_STATX
  struct statx stx;
  if (statx(AT_FDCWD, "/", AT_STATX_SYNC_AS_STAT, STATX_BTIME, &stx) == 0 &&
      (stx.stx_mask & STATX_BTIME) && stx.stx_btime.tv_sec > 0)
    return (long long)stx.stx_btime.tv_sec;
#endif
  return 0;
}

/* How long ago the distribution was installed, from the root filesystem's
 * creation time. Omitted when the filesystem does not report one. */
static void gather_age(void) {
  long long born = root_birth_time();
  if (born <= 0)
    return;
  long long secs = (long long)time(NULL) - born;
  if (secs < 0)
    return;

  long long days = secs / 86400;
  long long years = days / 365;
  long long months = (days % 365) / 30;
  long long rem_days = (days % 365) % 30;

  char val[128];
  if (years > 0)
    snprintf(val, sizeof(val), "%lld year%s, %lld month%s", years,
             years == 1 ? "" : "s", months, months == 1 ? "" : "s");
  else if (months > 0)
    snprintf(val, sizeof(val), "%lld month%s, %lld day%s", months,
             months == 1 ? "" : "s", rem_days, rem_days == 1 ? "" : "s");
  else
    snprintf(val, sizeof(val), "%lld day%s", days, days == 1 ? "" : "s");
  info_emit("Age", "%s", val);
}

static void gather_packages(void) {
  char val[256] = "";
  int n = 0;

#ifdef __APPLE__
  const char *cellars[] = {"/opt/homebrew/Cellar", "/usr/local/Cellar", NULL};
  int total = 0;
  for (int i = 0; cellars[i]; i++)
    total += count_subdirs(cellars[i]);
  if (total > 0)
    snprintf(val, sizeof(val), "%d (brew)", total);
#else
  n = count_emerge_pkgs();
  if (n > 0)
    snprintf(val, sizeof(val), "%d (emerge)", n);
  if (!val[0]) {
    n = count_subdirs("/var/lib/pacman/local");
    if (n > 1)
      snprintf(val, sizeof(val), "%d (pacman)", n - 1);
  }
  if (!val[0]) {
    n = count_file_lines("/var/lib/dpkg/status", "Package:");
    if (n > 0)
      snprintf(val, sizeof(val), "%d (dpkg)", n);
  }
  if (!val[0]) {
    FILE *fp = popen("rpm -qa 2>/dev/null", "r");
    if (fp) {
      n = 0;
      char buf[256];
      while (fgets(buf, sizeof(buf), fp))
        n++;
      pclose(fp);
      if (n > 0)
        snprintf(val, sizeof(val), "%d (rpm)", n);
    }
  }
  if (!val[0]) {
    FILE *fp = popen("xbps-query -l 2>/dev/null | wc -l", "r");
    if (fp) {
      if (fscanf(fp, "%d", &n) == 1 && n > 0)
        snprintf(val, sizeof(val), "%d (xbps)", n);
      pclose(fp);
    }
  }
  if (!val[0]) {
    n = count_file_lines("/lib/apk/db/installed", "P:");
    if (n > 0)
      snprintf(val, sizeof(val), "%d (apk)", n);
  }
  if (!val[0]) {
    n = count_subdirs("/var/lib/eopkg/package");
    if (n > 0)
      snprintf(val, sizeof(val), "%d (eopkg)", n);
  }
  if (!val[0] && access("/nix/store", F_OK) == 0) {
    /* Nix / NixOS: ask the store what the running system needs. Bounded so a
     * slow store cannot stall startup. */
    FILE *fp = popen("timeout 2 nix-store -q --requisites /run/current-system"
                     " 2>/dev/null | wc -l",
                     "r");
    if (fp) {
      if (fscanf(fp, "%d", &n) == 1 && n > 0)
        snprintf(val, sizeof(val), "%d (nix)", n);
      pclose(fp);
    }
  }
#ifdef __FreeBSD__
  if (!val[0]) {
    n = count_subdirs("/var/db/pkg");
    if (n > 0)
      snprintf(val, sizeof(val), "%d (pkg)", n);
  }
#endif
#endif

  if (val[0])
    info_emit("Packages", "%s", val);
}

static int looks_like_shell(const char *s) {
  static const char *shells[] = {"bash", "zsh",  "fish",   "dash", "sh",
                                 "nu",   "elvish", "tcsh", "csh",  "xonsh",
                                 "ksh",  "ion",  NULL};
  for (int i = 0; shells[i]; i++)
    if (strcmp(s, shells[i]) == 0)
      return 1;
  return 0;
}

static void gather_shell(void) {
  char shell_buf[64] = "";
  char *shell = NULL;

  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/comm", getppid());
  FILE *fp = fopen(path, "r");
  if (fp) {
    if (fgets(shell_buf, sizeof(shell_buf), fp)) {
      int len = (int)strlen(shell_buf);
      while (len > 0 && (shell_buf[len - 1] == '\n' || shell_buf[len - 1] == '\r'))
        shell_buf[--len] = '\0';
      if (looks_like_shell(shell_buf))
        shell = shell_buf;
    }
    fclose(fp);
  }
  if (!shell)
    shell = getenv("SHELL");
  if (!shell)
    return;

  char *name = strrchr(shell, '/');
  name = name ? name + 1 : shell;
  info_emit("Shell", "%s", name);
}

/* Copy the value after the ':' on a /proc/cpuinfo line whose key matches. */
static int cpuinfo_value(const char *line, const char *key, char *out,
                         size_t outlen) {
  size_t klen = strlen(key);
  if (strncmp(line, key, klen) != 0)
    return 0;
  const char *colon = strchr(line, ':');
  if (!colon)
    return 0;
  const char *val = colon + 1;
  while (*val == ' ' || *val == '\t')
    val++;
  size_t len = strcspn(val, "\r\n");
  if (len == 0 || len >= outlen)
    return 0;
  memcpy(out, val, len);
  out[len] = '\0';
  return 1;
}

static void gather_cpu(void) {
#ifdef __APPLE__
  FILE *fp = popen("sysctl -n machdep.cpu.brand_string 2>/dev/null", "r");
  char name[128] = "";
  if (fp) {
    if (fgets(name, sizeof(name), fp)) {
      int len = (int)strlen(name);
      while (len > 0 && (name[len - 1] == '\n' || name[len - 1] == '\r'))
        name[--len] = '\0';
    }
    pclose(fp);
  }
  if (name[0])
    info_emit("CPU", "%s", name);
#else
  char name[128] = "";
  char hw[128] = "";
  int cores = 0;
  float max_ghz = 0;

  FILE *fp = fopen("/proc/cpuinfo", "r");
  if (fp) {
    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) {
      /* x86 and most arm64 kernels use "model name"; some arm64 and 32-bit ARM
       * boards use "Model" or "Hardware" instead. Take the first of each. */
      if (!name[0] &&
          (cpuinfo_value(buf, "model name", name, sizeof(name)) ||
           cpuinfo_value(buf, "Model", name, sizeof(name)))) {
        char *at = strstr(name, " @ ");
        if (at)
          *at = '\0';
      }
      if (!hw[0])
        cpuinfo_value(buf, "Hardware", hw, sizeof(hw));
      if (strncmp(buf, "processor", 9) == 0)
        cores++;
    }
    fclose(fp);
  }

  /* Boards that report nothing useful in /proc/cpuinfo (most ARM SBCs and
   * Apple Silicon) expose a name through the device tree. Apple's entry is the
   * machine model, so keep only the product part; others are used verbatim. */
  if (!name[0]) {
    char model[128] = "";
    if (nx_read_first_line("/proc/device-tree/model", model, sizeof(model))) {
      if (strncmp(model, "Apple", 5) == 0) {
        char *paren = strchr(model, '(');
        if (paren) {
          paren++;
          char *comma = strchr(paren, ',');
          char *end = comma ? comma : strchr(paren, ')');
          if (end && end > paren)
            snprintf(name, sizeof(name), "Apple %.*s", (int)(end - paren), paren);
        }
      }
      if (!name[0])
        snprintf(name, sizeof(name), "%s", model);
    }
  }
  if (!name[0] && hw[0])
    snprintf(name, sizeof(name), "%s", hw);

  DIR *cpufreq = opendir("/sys/devices/system/cpu/cpufreq");
  if (cpufreq) {
    struct dirent *ent;
    long max_khz = 0;
    while ((ent = readdir(cpufreq))) {
      if (strncmp(ent->d_name, "policy", 6) != 0)
        continue;
      char path[128];
      snprintf(path, sizeof(path),
               "/sys/devices/system/cpu/cpufreq/%s/cpuinfo_max_freq",
               ent->d_name);
      FILE *f = fopen(path, "r");
      if (!f)
        continue;
      long khz = 0;
      if (fscanf(f, "%ld", &khz) == 1 && khz > max_khz)
        max_khz = khz;
      fclose(f);
    }
    closedir(cpufreq);
    if (max_khz > 0)
      max_ghz = max_khz / 1000000.0f;
  }

  if (name[0]) {
    if (cores > 0 && max_ghz > 0)
      info_emit("CPU", "%s (%d) @ %.2f GHz", name, cores, max_ghz);
    else if (cores > 0)
      info_emit("CPU", "%s (%d)", name, cores);
    else
      info_emit("CPU", "%s", name);
  }
#endif
}

static void gather_gpu(void) {
#ifndef __APPLE__
  DIR *d = opendir("/sys/class/drm");
  if (!d)
    return;
  struct dirent *ent;
  while ((ent = readdir(d))) {
    if (strncmp(ent->d_name, "card", 4) != 0)
      continue;
    int all_digits = ent->d_name[4] != '\0';
    for (int i = 4; ent->d_name[i]; i++)
      if (ent->d_name[i] < '0' || ent->d_name[i] > '9') {
        all_digits = 0;
        break;
      }
    if (!all_digits)
      continue;

    char path[256];
    snprintf(path, sizeof(path), "/sys/class/drm/%s/device/uevent", ent->d_name);
    FILE *fp = fopen(path, "r");
    if (!fp)
      continue;
    char driver[32] = "";
    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) {
      if (strncmp(buf, "DRIVER=", 7) == 0) {
        char *val = buf + 7;
        int l = (int)strlen(val);
        while (l > 0 && (val[l - 1] == '\n' || val[l - 1] == '\r'))
          val[--l] = '\0';
        strncpy(driver, val, sizeof(driver) - 1);
        driver[sizeof(driver) - 1] = '\0';
      }
    }
    fclose(fp);

    const char *name = NULL;
    if (strcmp(driver, "i915") == 0 || strcmp(driver, "xe") == 0)
      name = "Intel Graphics";
    else if (strcmp(driver, "amdgpu") == 0 || strcmp(driver, "radeon") == 0)
      name = "AMD Graphics";
    else if (strcmp(driver, "nvidia") == 0 || strcmp(driver, "nouveau") == 0)
      name = "NVIDIA GPU";
    /* ARM SoC GPUs: the DRM driver name says nothing to a user, so map the
     * common ones to their vendor. */
    else if (strcmp(driver, "vc4") == 0 || strcmp(driver, "v3d") == 0)
      name = "Broadcom VideoCore";
    else if (strcmp(driver, "msm") == 0)
      name = "Qualcomm Adreno";
    else if (strcmp(driver, "panfrost") == 0 || strcmp(driver, "panthor") == 0)
      name = "ARM Mali";
    else if (strcmp(driver, "lima") == 0)
      name = "ARM Mali";
    else if (strcmp(driver, "etnaviv") == 0)
      name = "Vivante GPU";
    else if (strcmp(driver, "rockchip") == 0)
      name = "ARM Mali";
    else if (driver[0] && strcmp(driver, "simpledrm") != 0 &&
             strcmp(driver, "simple-framebuffer") != 0)
      name = driver;

    if (name)
      info_emit("GPU", "%s", name);
  }
  closedir(d);
#endif
}

static void gather_memory(void) {
#ifndef __APPLE__
  long long total = 0, avail = 0;
  FILE *fp = fopen("/proc/meminfo", "r");
  if (!fp)
    return;
  char buf[128];
  while (fgets(buf, sizeof(buf), fp)) {
    if (strncmp(buf, "MemTotal:", 9) == 0)
      sscanf(buf + 9, " %lld", &total);
    else if (strncmp(buf, "MemAvailable:", 13) == 0)
      sscanf(buf + 13, " %lld", &avail);
  }
  fclose(fp);
  if (total <= 0)
    return;

  float used_gib = (total - avail) / 1048576.0f;
  float total_gib = total / 1048576.0f;
  int pct = (int)((total - avail) * 100 / total);
  const char *color = pct >= 80 ? "31" : pct >= 50 ? "93" : "32";
  info_emit("Memory", "%.2f GiB / %.2f GiB (\033[%sm%d%%\033[0m)", used_gib,
            total_gib, color, pct);
#endif
}

static void gather_swap(void) {
#ifdef __APPLE__
  FILE *fp = popen("sysctl -n vm.swapusage 2>/dev/null", "r");
  if (!fp)
    return;
  char swapstr[256] = "";
  if (fgets(swapstr, sizeof(swapstr), fp)) {
    float total_mb = 0, used_mb = 0;
    if (sscanf(swapstr, "total = %fM  used = %fM", &total_mb, &used_mb) == 2 &&
        total_mb > 0) {
      int pct = (int)(used_mb * 100 / total_mb);
      const char *color = pct >= 80 ? "31" : pct >= 50 ? "93" : "32";
      info_emit("Swap", "%.2f MiB / %.2f MiB (\033[%sm%d%%\033[0m)", used_mb,
                total_mb, color, pct);
    }
  }
  pclose(fp);
#else
  long long total = 0, free_s = 0;
  FILE *fp = fopen("/proc/meminfo", "r");
  if (!fp)
    return;
  char buf[128];
  while (fgets(buf, sizeof(buf), fp)) {
    if (strncmp(buf, "SwapTotal:", 10) == 0)
      sscanf(buf + 10, " %lld", &total);
    else if (strncmp(buf, "SwapFree:", 9) == 0)
      sscanf(buf + 9, " %lld", &free_s);
  }
  fclose(fp);
  if (total <= 0)
    return;
  long long used = total - free_s;
  int pct = (int)(used * 100 / total);
  const char *color = pct >= 80 ? "31" : pct >= 50 ? "93" : "32";
  if (total >= 1048576)
    info_emit("Swap", "%.2f GiB / %.2f GiB (\033[%sm%d%%\033[0m)",
              used / 1048576.0f, total / 1048576.0f, color, pct);
  else
    info_emit("Swap", "%.2f MiB / %.2f MiB (\033[%sm%d%%\033[0m)",
              used / 1024.0f, total / 1024.0f, color, pct);
#endif
}

static void gather_disk(void) {
  struct statvfs st;
  if (statvfs("/", &st) != 0 || st.f_blocks == 0)
    return;

  float total_gib = (float)st.f_blocks * (float)st.f_frsize / (1024 * 1024 * 1024);
  float free_gib = (float)st.f_bfree * (float)st.f_frsize / (1024 * 1024 * 1024);
  float used_gib = total_gib - free_gib;
  int pct = total_gib > 0 ? (int)(used_gib * 100 / total_gib) : 0;
  const char *color = pct >= 80 ? "31" : pct >= 50 ? "93" : "32";

  char fstype[32] = "";
  FILE *fp = fopen("/proc/mounts", "r");
  if (fp) {
    char buf[512];
    while (fgets(buf, sizeof(buf), fp)) {
      char dev[128], mnt[128], fs[32];
      if (sscanf(buf, "%127s %127s %31s", dev, mnt, fs) == 3 &&
          strcmp(mnt, "/") == 0) {
        strncpy(fstype, fs, sizeof(fstype) - 1);
        break;
      }
    }
    fclose(fp);
  }

  if (fstype[0])
    info_emit("Disk (/)", "%.2f GiB / %.2f GiB (\033[%sm%d%%\033[0m) - %s",
              used_gib, total_gib, color, pct, fstype);
  else
    info_emit("Disk (/)", "%.2f GiB / %.2f GiB (\033[%sm%d%%\033[0m)", used_gib,
              total_gib, color, pct);
}

/* -------------------------------------------------------------- palette */
/* Two rows of eight blocks showing the 16 standard terminal colours, with a
 * blank line above to set them apart from the fields. Colour 0-7 is drawn with
 * the normal background SGR codes and 8-15 with the bright ones. */
void info_add_palette(void) {
  add_line("");

  for (int row = 0; row < 2; row++) {
    char line[NEXUS_MAX_LINE_LEN];
    int pos = 0;
    for (int i = 0; i < 8; i++) {
      int c = row * 8 + i;
      int sgr = c < 8 ? 40 + c : 100 + (c - 8);
      pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "\033[%dm   ", sgr);
    }
    snprintf(line + pos, sizeof(line) - (size_t)pos, "\033[0m");
    add_line(line);
  }
}

/* -------------------------------------------------------------- dispatch */
void info_gather_all(void) {
  info_line_count = 0;
  gather_title();
  gather_os();
  gather_host();
  gather_kernel();
  gather_uptime();
  gather_age();
  gather_packages();
  gather_shell();
  gather_cpu();
  gather_gpu();
  gather_memory();
  gather_swap();
  gather_disk();
}
