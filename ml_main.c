#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "scanner.h"

void usage(const char *progname) {
    fprintf(stderr, "Usage: %s scan -p <path> [-d <depth>] [-e <ext>]\n", progname);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "scan") != 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Inicjalizacja struktury domyślnymi wartościami
    AppConfig config = { .path_count = 0, .depth = 1, .ext_count = 0, .output = stdout};
    int c;
    int use_env_output = 0;
    char *file_path_from_cli = NULL;

    // Parsowanie argumentów za pomocą getopt
    while ((c = getopt(argc - 1, argv + 1, "p:d:e:o:O")) != -1) {
        switch (c) {
            case 'p':
                if (config.path_count < MAX_PATH)
                    config.paths[config.path_count++] = optarg;
                break;
            case 'd':
                config.depth = (int)strtol(optarg, NULL, 10);
                break;
            case 'e':
                if (config.ext_count < MAX_PATH)
                    config.extensions[config.ext_count++] = optarg;
                break;
            case 'o':
                file_path_from_cli = optarg;
                break;
            case 'O':
                use_env_output = 1;
                break;
            default:
                usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    // wybieranie strumienia wyjsciowego!
    char *final_path = NULL;

    if (file_path_from_cli != NULL) {
        final_path = file_path_from_cli;
    } else if (use_env_output) {
        char* env_val = getenv("L1_OUTPUTFILE");
        if (env_val != NULL && strlen(env_val) > 0) {
            final_path = env_val;
        }
    }

    // otwieramy plik jesli uzywamy
    if (final_path != NULL) {
        config.output = fopen(final_path, "w");
        if (config.output == NULL) {
            perror("Error opening output file");
            return EXIT_FAILURE;
        }
    }

    if (config.path_count == 0) {
        fprintf(stderr, "Błąd: Nie podano żadnej ścieżki (-p).\n");
        return EXIT_FAILURE;
    }

    // Uruchomienie skanowania dla podanych ścieżek
    for (int i = 0; i < config.path_count; i++) {
        scan_dir(config.paths[i], 0, &config);
    }

    // zamykamy plik jesli trzeba
    if (config.output != stdout) {
        fclose(config.output);
    }

    return EXIT_SUCCESS;
}
