#ifndef MEGALAB_SCANNER_H
#define MEGALAB_SCANNER_H

#define MAX_PATH 1024

typedef struct {
    char *paths[MAX_PATH];
    int path_count;
    int depth;
    char *extensions[MAX_PATH];
    int ext_count;
    FILE *output;

} AppConfig;

void scan_dir(const char *path, int level, const AppConfig *config);

#endif //MEGALAB_SCANNER_H