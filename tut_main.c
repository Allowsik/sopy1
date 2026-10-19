#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PATH 101

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void tree_dir(const char* path, int level) {
    DIR *dir = opendir(path);
    if (dir == NULL) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        errno = 0;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        // printer
        for (int i = 0; i < level; i++) {
            printf("  ");
        }
        printf((entry->d_type == DT_DIR ? "%s/\n" : "%s\n"), entry->d_name);

        if (entry->d_type == DT_DIR) {
            char next_path[1024];
            snprintf(next_path, 1024, "%s/%s", path, entry->d_name);
            // recursion
            tree_dir(next_path, level + 1);
        }
    }
}

void scan_dir() {
    DIR *dir = opendir(".");
    if (dir == NULL) return;
    struct dirent *entry;
    struct stat buf;

    int dirs = 0;
    int files = 0;
    int links = 0;
    int others = 0;

    while ((entry = readdir(dir)) != NULL) {
        errno = 0;
        if (strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".") == 0) continue;
        if (lstat(entry->d_name, &buf)) ERR("lstat");

        if (S_ISDIR(buf.st_mode)) dirs++;
        else if (S_ISREG(buf.st_mode)) files++;
        else if (S_ISLNK(buf.st_mode)) links++;
        else others++;

    }
    if (errno != 0) ERR("readdir");
    if (closedir(dir)) ERR("closedir");
    printf("Files: %d, Dirs: %d, Links: %d, Other: %d\n", files, dirs, links, others);

}

void usage(char *pname)
{
    fprintf(stderr, "USAGE:%s -n Name -p OCTAL -s SIZE\n", pname);
    exit(EXIT_FAILURE);
}

void make_file(char *name, ssize_t size, mode_t perms, int percent) {
    FILE *f = fopen(name, "w");
    if (f == NULL) ERR("fopen");

    for (int i = 0; i < size; i++) {
        int znak = (rand() % 10 == 0) ? 'A' + rand() % 26 : '\0';
        if (fputc(znak, f) == EOF) ERR("fputc");
    }

    fclose(f);
    if (chmod(name, perms) == -1) ERR("chmod");
}


int main(int argc, char *argv[]) {
    int c;
    char *name = NULL;
    mode_t perms = -1;
    ssize_t size = -1;

    while ((c = getopt(argc, argv, "p:n:s:")) != -1)
        switch (c) {
            case 'p':
                perms = strtol(optarg, (char **)NULL, 8);
            case 's':
                size = strtol(optarg, (char **)NULL, 10);
                break;
            case 'n':
                name = optarg;
                break;
            case '?':
            default:
                usage(argv[0]);
        }
    if ((NULL == name) || (perms == (mode_t)-1) || (size == -1))
        usage(argv[0]);
    if (unlink(name) && errno != ENOENT)
        ERR("unlink");
    srand(time(NULL));
    make_file(name, size, perms, 10);

    /*char path[MAX_PATH];
    if (getcwd(path, MAX_PATH) == NULL) ERR("getcwd");
    for (int i = 1; i < argc; i++) {
        if (chdir(argv[i])) ERR("chdir");
        printf("%s\n", argv[i]);
        scan_dir();
        if (chdir(path)) ERR("chdir");
    }*/

    //const int level = 0;
    //tree_dir(path, level);

    /*
    printf("--------------------------\n");
    printf("Scanning current dir\n");
    scan_dir();

    printf("--------------------------\n");
    printf("Test %d\n", chdir(path));
    */


    return EXIT_SUCCESS;
}