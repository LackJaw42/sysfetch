/*
 * sysfetch.c — Waybar custom module: CPU temp, memory usage, battery percent
 *
 * Uses FontAwesome icons (already in your waybar font stack).
 *
 * Compile & install:
 *   gcc -O2 -o sysfetch sysfetch.c
 *   sudo cp sysfetch /usr/local/bin/sysfetch
 *
 * Then reload waybar:
 *   killall waybar && waybar &
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

/* ─── helpers ────────────────────────────────────────────────────────────── */

static long read_long(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    long val = 0;
    if (fscanf(f, "%ld", &val) != 1) val = 0;
    fclose(f);
    return val;
}

static int read_int(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int val = -1;
    if (fscanf(f, "%d", &val) != 1) val = -1;
    fclose(f);
    return val;
}

static int read_str(const char *path, char *buf, size_t len)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char *ret = fgets(buf, (int)len, f);
    fclose(f);
    if (!ret) return 0;
    size_t n = strlen(buf);
    if (n && buf[n - 1] == '\n') buf[n - 1] = '\0';
    return 1;
}

/* ─── CPU temperature ────────────────────────────────────────────────────── */

static double read_cpu_temp(void)
{
    DIR *d = opendir("/sys/class/thermal");
    if (d) {
        struct dirent *e;
        double best = -1.0;
        int found_cpu = 0;
        while ((e = readdir(d)) != NULL) {
            if (strncmp(e->d_name, "thermal_zone", 12) != 0) continue;
            char path[512];
            snprintf(path, sizeof path, "/sys/class/thermal/%s/type", e->d_name);
            char type[64] = {0};
            read_str(path, type, sizeof type);
            int is_cpu = (strstr(type, "cpu")  || strstr(type, "CPU")  ||
                          strstr(type, "x86")  || strstr(type, "acpi") ||
                          strstr(type, "core") || strstr(type, "Core"));
            snprintf(path, sizeof path, "/sys/class/thermal/%s/temp", e->d_name);
            long raw = read_long(path);
            if (!raw) continue;
            double celsius = raw / 1000.0;
            if (is_cpu && !found_cpu) { best = celsius; found_cpu = 1; }
            else if (best < 0.0)      { best = celsius; }
        }
        closedir(d);
        if (best >= 0.0) return best;
    }

    DIR *d2 = opendir("/sys/class/hwmon");
    if (d2) {
        struct dirent *e;
        while ((e = readdir(d2)) != NULL) {
            if (strncmp(e->d_name, "hwmon", 5) != 0) continue;
            for (int i = 1; i <= 9; i++) {
                char path[512];
                snprintf(path, sizeof path,
                         "/sys/class/hwmon/%s/temp%d_input", e->d_name, i);
                long raw = read_long(path);
                if (raw) { closedir(d2); return raw / 1000.0; }
            }
        }
        closedir(d2);
    }
    return -1.0;
}

/* ─── memory usage ───────────────────────────────────────────────────────── */

static double read_mem_percent(unsigned long *used_mb, unsigned long *total_mb)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1.0;
    unsigned long mem_total = 0, mem_available = 0;
    char line[128];
    while (fgets(line, sizeof line, f)) {
        unsigned long val;
        if (sscanf(line, "MemTotal: %lu kB",     &val) == 1) mem_total     = val;
        if (sscanf(line, "MemAvailable: %lu kB", &val) == 1) mem_available = val;
        if (mem_total && mem_available) break;
    }
    fclose(f);
    if (!mem_total) return -1.0;
    unsigned long used = mem_total - mem_available;
    if (used_mb)  *used_mb  = used      / 1024;
    if (total_mb) *total_mb = mem_total / 1024;
    return (double)used / (double)mem_total * 100.0;
}

/* ─── battery ────────────────────────────────────────────────────────────── */

static int read_battery(char *status, size_t status_len)
{
    DIR *d = opendir("/sys/class/power_supply");
    if (!d) return -1;
    struct dirent *e;
    char bat_path[512] = {0};
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "BAT", 3) == 0 ||
            strncmp(e->d_name, "CMB", 3) == 0) {
            snprintf(bat_path, sizeof bat_path,
                     "/sys/class/power_supply/%s", e->d_name);
            break;
        }
    }
    closedir(d);
    if (!bat_path[0]) return -1;

    char cap_path[600];
    snprintf(cap_path, sizeof cap_path, "%s/capacity", bat_path);
    int cap = read_int(cap_path);

    if (status) {
        char stat_path[600];
        snprintf(stat_path, sizeof stat_path, "%s/status", bat_path);
        if (!read_str(stat_path, status, status_len))
            strncpy(status, "Unknown", status_len - 1);
    }
    return cap;
}

/* ─── CSS class ──────────────────────────────────────────────────────────── */

static const char *get_class(double cpu_temp, double mem_pct, int bat_pct)
{
    if ((cpu_temp >= 0.0 && cpu_temp >= 85.0) ||
        (mem_pct  >= 0.0 && mem_pct  >= 90.0) ||
        (bat_pct  >= 0   && bat_pct  <= 10))
        return "critical";
    if ((cpu_temp >= 0.0 && cpu_temp >= 70.0) ||
        (mem_pct  >= 0.0 && mem_pct  >= 75.0) ||
        (bat_pct  >= 0   && bat_pct  <= 20))
        return "warning";
    return "ok";
}

/* ─── main ───────────────────────────────────────────────────────────────── */

int main(void)
{
    double cpu_temp = read_cpu_temp();

    unsigned long used_mb = 0, total_mb = 0;
    double mem_pct = read_mem_percent(&used_mb, &total_mb);

    char bat_status[32] = "Unknown";
    int  bat_pct = read_battery(bat_status, sizeof bat_status);

	char temp_str[64], mem_str[64], bat_str[64];

    /* CPU temp */
    if (cpu_temp >= 0.0)
        snprintf(temp_str, sizeof temp_str, "CPU: %.0f°C", cpu_temp);
    else
        snprintf(temp_str, sizeof temp_str, "CPU: N/A");

    /* Memory */
    if (mem_pct >= 0.0)
        snprintf(mem_str, sizeof mem_str, "MEM: %.0f%%", mem_pct);
    else
        snprintf(mem_str, sizeof mem_str, "MEM: N/A");

    /* Battery */
    if (bat_pct < 0) {
        snprintf(bat_str, sizeof bat_str, "BAT: N/A");
    } else {
        const char *status_label = "";
        if (strcmp(bat_status, "Charging") == 0)   status_label = " CHR";
        else if (strcmp(bat_status, "Full") == 0)  status_label = " FULL";
        snprintf(bat_str, sizeof bat_str, "BAT: %d%%%s", bat_pct, status_label);
    }

    /* compose text */
    char text[256];
    snprintf(text, sizeof text, "%s | %s | %s |", temp_str, mem_str, bat_str);

    /* tooltip */
    char tip_cpu[64], tip_mem[128], tip_bat[64], tooltip[512];

    if (cpu_temp >= 0.0)
        snprintf(tip_cpu, sizeof tip_cpu, "CPU Temp: %.1f\xc2\xb0""C", cpu_temp);
    else
        snprintf(tip_cpu, sizeof tip_cpu, "CPU Temp: unavailable");

    if (mem_pct >= 0.0)
        snprintf(tip_mem, sizeof tip_mem,
                 "Memory: %lu MB / %lu MB (%.1f%%)", used_mb, total_mb, mem_pct);
    else
        snprintf(tip_mem, sizeof tip_mem, "Memory: unavailable");

    if (bat_pct >= 0)
        snprintf(tip_bat, sizeof tip_bat, "Battery: %d%% (%s)", bat_pct, bat_status);
    else
        snprintf(tip_bat, sizeof tip_bat, "Battery: not detected");

    snprintf(tooltip, sizeof tooltip, "%s\\n%s\\n%s", tip_cpu, tip_mem, tip_bat);

    printf("{\"text\":\"%s\",\"tooltip\":\"%s\",\"class\":\"%s\"}\n",
           text, tooltip, get_class(cpu_temp, mem_pct, bat_pct));

    return 0;
}
