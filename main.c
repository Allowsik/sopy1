#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define MAX_VALS 10

int main(int argc, char *argv[]) {
    char *paths[MAX_VALS];
    int path_count = 0;
    char* out = NULL;
    int o_count = 0;
    int printFile = 0;

    // Detect arguments
    int c;
    while((c=getopt(argc, argv, "p:o:")) != -1) {
        switch(c) {
            case 'p':
                if (path_count < MAX_VALS) {
                    paths[path_count++] = optarg;
                }
                break;
            case 'o':
                o_count++;
                if (o_count > 1) ERR("too many arguments");
                out = optarg;
                printFile = 1;
                break;
            default:
                printf("Usage: %s [-p path] [-o out]\n", argv[0]);
                break;
        }
    }

    struct dirent *dp;

    FILE *f;
    if (printFile) {
        f = fopen(out, "w");
        if (!f) ERR("fopen");
    }

    for (int i=0; i<path_count; i++) {
        DIR *dirp = opendir(paths[i]);
        if (dirp == NULL) continue;
        if (!printFile) {
            printf("SCIEZKA:\n%s\nLISTA PLIKÓW:\n", paths[i]);
        } else {
            fprintf(f, "SCIEZKA:\n%s\nLISTA PLIKÓW:\n", paths[i]);
        }

        while ((dp = readdir(dirp)) != NULL) {
            if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) continue;
            char full_path[1024];
            snprintf(full_path, 1024, "%s/%s", paths[i], dp->d_name);
            struct stat st;
            if (lstat(full_path, &st) == -1) ERR("lstat");
            if (!printFile) {
                printf("%s %ld\n", dp->d_name, st.st_size);
            } else {
                fprintf(f, "%s %ld\n", dp->d_name, st.st_size);
            }
        }
        if (closedir(dirp)) ERR("closedir");
    }
    if (printFile && f != NULL)
        if (fclose(f))
            ERR("fclose");

    return EXIT_SUCCESS;
}
