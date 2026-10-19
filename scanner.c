#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include "scanner.h"

// static oznacza, że funkcja jest "prywatna" i niewidoczna poza tym plikiem
static int match_extension(const char *filename, char * const exts[], int ext_count) {
    if (ext_count == 0) return 1;

    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return 0;

    const char *ext = dot + 1;
    for (int i = 0; i < ext_count; i++) {
        if (strcmp(ext, exts[i]) == 0) return 1;
    }
    return 0;
}

void scan_dir(const char *path, int level, const AppConfig *config) {
    DIR *dir = opendir(path);
    if (dir == NULL) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat filestat;
        if (lstat(full_path, &filestat) != 0) continue;

        int is_dir = S_ISDIR(filestat.st_mode);

        // Filtrowanie z wykorzystaniem konfiguracji ze struktury
        if (!is_dir && !match_extension(entry->d_name, config->extensions, config->ext_count)) {
            continue;
        }

        // Rysowanie wcięć
        for (int i = 0; i < level; i++) fprintf(config->output, "  ");

        if (is_dir) {
            fprintf(config->output, "%s/\n", entry->d_name);
        } else {
            fprintf(config->output, "%s %ld\n", entry->d_name, (long)filestat.st_size);
        }

        // Rekurencja - przekazujemy wskaźnik na config dalej
        if (is_dir && level < config->depth) {
            scan_dir(full_path, level + 1, config);
        }
    }
    closedir(dir);
}
