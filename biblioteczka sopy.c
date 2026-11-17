#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define _POSIX_C_SOURCE 200809L
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))


#define MAX_LINE 512
int main(void) {
    printf("Hello, World!\n");
    return 0;
}

/*
 * 0) Niektóre funkcje
 * 1) Jeśli istnieje katalog to wypisz cały katalog, jeśli katalog nie istnieje, tworzy go z uprawnieniami i tworzy x plików
 * 2) Otwórz plik i wpisz x losowych znaków
 * 3) Zmień uprawnienia do pliku
 * 4) Stwórz katalog
 * 5) Wypisz cały katalog razem z rozmiarami obiektów do pliku
 * 6) Zadanie 1.1 z obsługą parametrów
 * 7) Zadanie 1.2 (rekurencyjne wypisywanie obiektów w folderze)
 * 8) Zadanie 1.3 Dodawanie do requirements nazwę i versję, usuwanie pakietu, wykonywanie wszystkiego na kilku środowiskach
 * 8.1) Tworzy nowe środowisko: katalog <env> oraz pusty plik <env>/requirements
 * 8.2) Sprawdza, czy katalog środowiska istnieje
 * 8.3) Sprawdza, czy pakiet jest już wpisany w pliku requirements
 * 8.4) Instaluje pakiet pkg_spec (np. "numpy==1.0.0") w środowisku env
 * 8.5) Usuwa pakiet pkg_name ze środowiska env (rename, podmiana plików)
 * 8.6) Osbługa flag
 * 9) Kopiowanie plików używając funkcji niskopoziomowych (open, read, write)
 * 10) Kopiowanie informacji z plików binarnych używając funkcji niskopoziomowych (iov_base, readv, writev)
 * 11) Kopiowanie informacji z plików binarnych używając funkcji mmap
 * 12) Zapisanie informacji z kilku deskryptorów przy użyciu polla do pliku
 * 13) Tworzenie x fifo (Łącza nazwane)
 * 14.1) Zad 2.1 etap 1, Weryfikacja rozmiaru pliku używając lseek i tworzenie pliku wypełnionego zerami, jedno fifo otwarte
 * 14.2) Zad 2.1 etap 2 parsowanie wiadomości na kilka razy
 * 14.3) Zad 2.1 etap 3 Kilka deskryptorów przy użyciu poll
 * 14.4) Zad 2.1 etap 4 Użycie mmap oraz zamykanie fifo
*/

//========================================================================================================================
//0) Getopt - parsowanie flag z linii poleceń
//========================================================================================================================

/*
int getopt(int argc, char * const argv[], const char *optstring);
optstring – ciąg znaków definiujący dozwolone flagi. Jeśli po literze występuje dwukropek (:), oznacza to, że opcja wymaga argumentu.
optarg - char * do następnych parametrów
optind - indeks następnego elementu do przetwożenia (startuje z 1 bo argv[0] to nazwa programu)
*/



//========================================================================================================================
//1) Jeśli istnieje katalog to wypisz cały katalog, jeśli katalog nie istnieje, tworzy go z uprawnieniami i tworzy x plików
//========================================================================================================================

void process_or_create_dir(const char *path, mode_t dir_perms, int x, mode_t file_perms) // *path - string, mode_t - uprawnienia -> (mode_t a = 0777;)
{
    struct stat st;

    /* 1. Sprawdzenie, czy ścieżka istnieje i jest katalogiem */
    if (stat(path, &st) == 0) //czy pobranie danych o obiekcie w path zakończyło się sukcesem
    {
        if (!S_ISDIR(st.st_mode)) //Czy to jest katalog
        {
            fprintf(stderr, "Ścieżka '%s' istnieje, ale nie jest katalogiem.\n", path);
            return;
        }

        //Zapisujemy skąd startujemy
        char saved_cwd[1024];
        if (getcwd(saved_cwd, sizeof(saved_cwd)) == NULL)
            ERR("getcwd");

        //Idziemy do nowego katalogu
        if (chdir(path) == -1)
            ERR("chdir");
        //otwieramy katalog
        DIR *dirp = opendir(".");
        if (dirp == NULL)
            ERR("opendir");

        struct dirent *dp;
        struct stat entry_stat;

        printf("Pliki w katalogu '%s':\n", path);
        while (1)
        {
            errno = 0;
            dp = readdir(dirp);
            if (dp == NULL) //jeżeli doszliśmy do końca
                break;

            /* Pominięcie wpisów '.' oraz '..' */
            if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
                continue;

            if (lstat(dp->d_name, &entry_stat) == -1) //lstat zapisuje informacje o samym obiekcie, (info o symlinku a nie o tym co jest pod nim)
                ERR("lstat");

            /* Wypisanie wyłącznie zwykłych plików */
            if (S_ISREG(entry_stat.st_mode)) // jeżeli jest plik regularny (nie urządzenia, symlinki, łącza nazwane i katalogi)
                printf("  %s\n", dp->d_name);
        }

        if (errno != 0)
            ERR("readdir");
        if (closedir(dirp) == -1)
            ERR("closedir");

        /* Powrót do pierwotnego katalogu roboczego */
        if (chdir(saved_cwd) == -1)
            ERR("chdir");
    }
    /* 2. Ścieżka nie istnieje (ENOENT) -> tworzymy katalog i x plików */
    else if (errno == ENOENT)
    {
        /* Uwaga: katalog potrzebuje bitu wykonania (x), aby można było do niego wejść */
        if (mkdir(path, dir_perms) == -1)
            ERR("mkdir");

        char file_path[1024];
        for (int i = 0; i < x; i++)
        {
            //Funkcja snprintf bezpiecznie buduje sformatowany napis i zapisuje go do bufora file_path
            snprintf(file_path, sizeof(file_path), "%s/plik_%d.txt", path, i + 1);

            int fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, file_perms);
            if (fd == -1)
                ERR("open");

            if (close(fd) == -1)
                ERR("close");
        }
        printf("Utworzono katalog '%s' oraz %d plików.\n", path, x);
    }
    /* Inny błąd podczas sprawdzania ścieżki (np. brak uprawnień EACCES) */
    else
    {
        ERR("stat");
    }
}
//========================================================================================================================
//2) Otwórz plik i wpisz x losowych znaków
//========================================================================================================================


void write_random_chars(const char *path, int x)
{
    FILE *f = fopen(path, "w");
    if (f == NULL)
        ERR("fopen");

    for (int i = 0; i < x; i++)
    {
        char random_char = 'A' + (rand() % ('Z' - 'A' + 1));
        if (fputc(random_char, f) == EOF)
            ERR("fputc");
    }

    if (fclose(f) != 0)
        ERR("fclose");
}

//========================================================================================================================
//3) Zmień uprawnienia do pliku
//========================================================================================================================


void change_file_permissions(const char *path, mode_t perms)
{
    if (chmod(path, perms) == -1)
        ERR("chmod");
}

//========================================================================================================================
//4) Stwórz katalog
//========================================================================================================================


void create_directory(const char *path, mode_t perms)
{
    if (mkdir(path, perms) == -1)
        ERR("mkdir");
}

//========================================================================================================================
//5) Wypisz cały katalog razem z rozmiarami obiektów do pliku
//========================================================================================================================


void list_cwd(FILE *out)
{
    DIR *dirp = opendir(".");
    if (dirp == NULL)
        ERR("opendir");

    struct dirent *dp;
    struct stat filestat;

    fprintf(out, "SCIEZKA:\n.\nLISTA PLIKÓW:\n");

    for (;;)
    {
        errno = 0;
        dp = readdir(dirp);
        if (dp == NULL)
            break;

        /* Pominięcie wpisów '.' oraz '..' */
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
            continue;

        if (lstat(dp->d_name, &filestat) == -1)
            ERR("lstat");

        fprintf(out, "%s %ld\n", dp->d_name, (long)filestat.st_size);
    }

    if (errno != 0)
        ERR("readdir");
    if (closedir(dirp) == -1)
        ERR("closedir");
}

//========================================================================================================================
//6) Zadanie 1.1 z obsługą parametrów
//========================================================================================================================


int main(int argc, char **argv)
{
    int opt;
    const char *out_path = NULL;
    const char **paths = malloc(argc * sizeof(char *));
    if (!paths)
        exit(EXIT_FAILURE);
    int path_count = 0;

    while ((opt = getopt(argc, argv, "p:o:")) != -1)
    {
        switch (opt)
        {
            case 'p': //jeśli jest p
                paths[path_count++] = optarg; //zapisz aktualną ścieżkę
                break;

            case 'o': //jeśli jest o
                if (out_path != NULL) //jeśli już było jedno o
                {
                    free(paths);
                    usage(argv[0]);
                }
                out_path = optarg; //zapisz ścieżkę do zapisania
                break;

            case '?': //jeśli dostaliśmy nieznaną flagę
            default: //w każdym innym przypadku
                free(paths);
                usage(argv[0]);
        }
    }

    if (optind < argc)
    {
        free(paths);
        usage(argv[0]);
    }

    FILE *out = stdout;
    if (out_path != NULL)
    {
        out = fopen(out_path, "w");
        if (out == NULL)
        {
            perror("fopen");
            free(paths);
            exit(EXIT_FAILURE);
        }
    }

    if (path_count == 0)
    {
        // scan_dir(".", out);
    }
    else
    {
        for (int i = 0; i < path_count; i++)
        {
            // scan_dir(paths[i], out);
        }
    }

    if (out != stdout && fclose(out) != 0)
    {
        perror("fclose");
        free(paths);
        exit(EXIT_FAILURE);
    }

    free(paths);
    return EXIT_SUCCESS;
}


//========================================================================================================================
//7) Zadanie 1.2 (rekurencyjne wypisywanie obiektów w folderze
//========================================================================================================================

void list_dir_depth(const char *dir_path, int d)
{
    DIR *dirp = opendir(dir_path);
    if (dirp == NULL)
    {
        perror(dir_path);
        return;
    }

    printf("path: %s\n", dir_path);

    struct dirent *dp;
    struct stat filestat;
    char full_path[1024];

    while (1) //wypisz wszystko w folderze
    {
        errno = 0;
        dp = readdir(dirp);
        if (dp == NULL)
            break;

        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
            continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, dp->d_name);

        if (lstat(full_path, &filestat) == -1)
            ERR("lstat");

        if (S_ISREG(filestat.st_mode))
            printf("%s %ld\n", dp->d_name, (long)filestat.st_size);
    }

    if (errno != 0)
        ERR("readdir");

    /* Jeśli nie osiągnięto limitu głębokości d > 1, wchodzimy w podkatalogi */
    if (d > 1)
    {
        rewinddir(dirp); //wracamy na początek katalodu

        while (1)
        {
            errno = 0;
            dp = readdir(dirp);
            if (dp == NULL)
                break;

            if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
                continue;

            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, dp->d_name); //zapisujemy nową ścieżkę do pliku

            if (lstat(full_path, &filestat) == -1)
                ERR("lstat");

            if (S_ISDIR(filestat.st_mode)) //jeżeli jest to folder to wypisujemy
                list_dir_depth(full_path, d-1);
        }

        if (errno != 0)
            ERR("readdir");
    }

    if (closedir(dirp) == -1)
        ERR("closedir");
}


//========================================================================================================================
//8) Zadanie 1.3 Dodawanie do requirements nazwę i versję, usuwanie pakietu, wykonywanie wszystkiego na kilku środowiskach
//========================================================================================================================
void zad8()
{
/*8.1) Tworzy nowe środowisko: katalog <env> oraz pusty plik <env>/requirements */
void create_env(const char *env, const char *pname)
{
    if (mkdir(env, 0755) == -1)
    {
        if (errno == EEXIST)
            fatal_msg("the environment already exists", pname);
        ERR("mkdir");
    }

    char req_path[MAX_LINE];
    snprintf(req_path, sizeof(req_path), "%s/requirements", env);

    FILE *f = fopen(req_path, "w");
    if (f == NULL)
        ERR("fopen");
    if (fclose(f) != 0)
        ERR("fclose");
}


/* 8.2) Sprawdza, czy katalog środowiska istnieje */
void check_env_exists(const char *env, const char *pname)
{
    struct stat st;
    if (stat(env, &st) == -1 || !S_ISDIR(st.st_mode))
        fatal_msg("the environment does not exist", pname);
}

/* 8.3 Sprawdza, czy pakiet jest już wpisany w pliku requirements */
int is_package_installed(const char *req_path, const char *pkg_name)
{
    FILE *f = fopen(req_path, "r");
    if (f == NULL)
        ERR("fopen");

    char line[MAX_LINE];
    char existing_pkg[MAX_LINE];
    int found = 0;

    while (fgets(line, sizeof(line), f) != NULL)
    {    //%511s, wczytaj 511 znaków bo 512 znaków ma tablica a musi być jeszcze '\o'
        if (sscanf(line, "%511s", existing_pkg) == 1 && strcmp(existing_pkg, pkg_name) == 0)
        {
            found = 1;
            break;
        }
    }

    if (fclose(f) != 0)
        ERR("fclose");
    return found;
}

/* 8.4 Instaluje pakiet pkg_spec (np. "numpy==1.0.0") w środowisku env */
void install_package(const char *env, const char *pkg_spec, const char *pname) //pname to argv[0] do wypisywania błędów, *pkg_spec to nazwa i wersja pakietu
{
    check_env_exists(env, pname);

    char spec_copy[MAX_LINE];
    snprintf(spec_copy, sizeof(spec_copy), "%s", pkg_spec);

    char *eq = strstr(spec_copy, "=="); //sprawdzamy czy user podał odpowiedni format
    if (eq == NULL || eq == spec_copy || *(eq + 2) == '\0')
        fatal_msg("invalid package format (expected package==version)", pname);

    *eq = '\0'; //dzielimy pkg_spec na dwa stringi
    const char *pkg_name = spec_copy;
    const char *pkg_ver = eq + 2;

    char req_path[MAX_LINE];
    snprintf(req_path, sizeof(req_path), "%s/requirements", env);

    if (is_package_installed(req_path, pkg_name))
        fatal_msg("package is already installed", pname);

    /* Dopisanie nazwy i wersji do pliku requirements */
    FILE *req_f = fopen(req_path, "a");
    if (req_f == NULL)
        ERR("fopen");
    fprintf(req_f, "%s %s\n", pkg_name, pkg_ver);
    if (fclose(req_f) != 0)
        ERR("fclose");

    /* Utworzenie pliku pakietu z uprawnieniami tylko do odczytu (-r--r--r--) */
    char pkg_path[MAX_LINE];
    snprintf(pkg_path, sizeof(pkg_path), "%s/%s", env, pkg_name);

    mode_t old_mask = umask(~0444 & 0777);
    FILE *pkg_f = fopen(pkg_path, "w");
    umask(old_mask);

    if (pkg_f == NULL)
        ERR("fopen");
    fprintf(pkg_f, "%s==%s\n", pkg_name, pkg_ver);
    if (fclose(pkg_f) != 0)
        ERR("fclose");
}

/* 8.5 Usuwa pakiet pkg_name ze środowiska env (rename, podmiana plików)*/
void remove_package(const char *env, const char *pkg_name, const char *pname)
{
    check_env_exists(env, pname);

    char req_path[MAX_LINE];
    snprintf(req_path, sizeof(req_path), "%s/requirements", env);

    if (!is_package_installed(req_path, pkg_name))
        fatal_msg("package is not installed", pname);

    /* Przepisanie pliku requirements bez usuwanego pakietu przez plik tymczasowy */
    char tmp_path[MAX_LINE];
    snprintf(tmp_path, sizeof(tmp_path), "%s/requirements.tmp", env);

    FILE *src = fopen(req_path, "r");
    FILE *dst = fopen(tmp_path, "w");
    if (src == NULL || dst == NULL)
        ERR("fopen");

    char line[MAX_LINE];
    char existing_pkg[MAX_LINE];
    while (fgets(line, sizeof(line), src) != NULL)
    {
        if (sscanf(line, "%511s", existing_pkg) == 1 && strcmp(existing_pkg, pkg_name) == 0)
            continue;
        fputs(line, dst);
    }

    if (fclose(src) != 0 || fclose(dst) != 0)
        ERR("fclose");
    if (rename(tmp_path, req_path) == -1)
        ERR("rename");

    /* Usunięcie pliku pakietu */
    char pkg_path[MAX_LINE];
    snprintf(pkg_path, sizeof(pkg_path), "%s/%s", env, pkg_name);
    if (unlink(pkg_path) == -1)
        ERR("unlink");
}

/* 8.6 Osbługa flag*/
int main(int argc, char **argv)
{
    int opt;
    int create_flag = 0;
    const char *install_pkg = NULL;
    const char *remove_pkg = NULL;

    const char **envs = malloc(argc * sizeof(char *));
    if (envs == NULL)
        ERR("malloc");
    int env_count = 0;

    /* Parsowanie flag w dowolnej kolejności */
    while ((opt = getopt(argc, argv, "cv:i:r:")) != -1)
    {
        switch (opt)
        {
            case 'c':
                create_flag = 1;
                break;
            case 'v':
                envs[env_count++] = optarg;
                break;
            case 'i':
                if (install_pkg != NULL)
                {
                    free(envs);
                    usage(argv[0]);
                }
                install_pkg = optarg;
                break;
            case 'r':
                if (remove_pkg != NULL)
                {
                    free(envs);
                    usage(argv[0]);
                }
                remove_pkg = optarg;
                break;
            case '?':
            default:
                free(envs);
                usage(argv[0]);
        }
    }

    /* Brak lub nadmiarowe argumenty pozycyjne */
    if (optind < argc || env_count == 0)
    {
        free(envs);
        usage(argv[0]);
    }

    /* Tryb tworzenia nowego środowiska (-c pozwala tylko na jedno -v i bez -i/-r) */
    if (create_flag)
    {
        if (env_count != 1 || install_pkg != NULL || remove_pkg != NULL)
        {
            free(envs);
            usage(argv[0]);
        }
        create_env(envs[0], argv[0]);
        free(envs);
        return EXIT_SUCCESS;
    }

    /* Musi być podana co najmniej jedna operacja: -i lub -r */
    if (install_pkg == NULL && remove_pkg == NULL)
    {
        free(envs);
        usage(argv[0]);
    }

    /* Wykonanie operacji dla wszystkich podanych środowisk -v */
    for (int i = 0; i < env_count; i++)
    {
        if (install_pkg != NULL)
            install_package(envs[i], install_pkg, argv[0]);
        if (remove_pkg != NULL)
            remove_package(envs[i], remove_pkg, argv[0]);
    }

    free(envs);
    return EXIT_SUCCESS;
}


}



//========================================================================================================================
//9) Kopiowanie plików używając funkcji niskopoziomowych (open, read, write)
//========================================================================================================================

int main(const int argc, const char *const *const argv)
{
    if (argc != 3)
        usage(argv[0]);

    const char *const path_1 = argv[1];
    const char *const path_2 = argv[2];

    const int fd_1 = open(path_1, O_RDONLY);
    if (fd_1 == -1)
        ERR("open");

    const int fd_2 = open(path_2, O_WRONLY | O_CREAT, 0777);
    if (fd_2 == -1)
        ERR("open");

    char file_buf[FILE_BUF_LEN];
    while (1)
    {
        const ssize_t read_size = read(fd_1, file_buf, FILE_BUF_LEN);
        if (read_size == -1)
            ERR("read");

        if (read_size == 0)
            break;

        if (write(fd_2, file_buf, read_size) == -1)
            ERR("write");
    }

    if (close(fd_2) == -1)
        ERR("close");

    if (close(fd_1) == -1)
        ERR("close");

    return EXIT_SUCCESS;
}


//========================================================================================================================
//10) Kopiowanie informacji z plików binarnych używając funkcji niskopoziomowych (iov_base, readv, writev)
//========================================================================================================================

typedef struct
{
    unsigned int size;
    char title[65];
} Book;

int main(int argc, char **argv)
{
    if (argc != 3)
        usage(argv[0]);

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0)
        ERR("open");

    struct stat s; //wyznaczamy rozmiar pliku
    if (fstat(fd, &s) < 0)
        ERR("fstat");

    const off_t size = s.st_size;
    const int N = size / 68;

    struct iovec *buffers = malloc(N * sizeof(struct iovec)); //iovec - struktura używana w wektorowych operacjach wejścia/wyjścia do opisywania pojedyńczego bufroa w pamięci
    Book *books = malloc(N * sizeof(Book));
    if (buffers == NULL || books == NULL)
        ERR("malloc");

    for (int i = 0; i < N; i++)
    {
        buffers[i].iov_base = &books[i]; //iov_base - wskaźnik na adres pamięci
        buffers[i].iov_len = 68; //iov_len - liczba bajtów do odczytania
        books[i].title[64] = '\0';
    }

    if (readv(fd, buffers, N) < size) //jeśli odczytaliśmy za mało bajtów
        ERR("readv");
    if (close(fd) < 0)
        ERR("close");

    for (int i = 0; i < N; i++)
    {
        printf("Size: %u, Title: %s\n", books[i].size, books[i].title);
        books[i].size++;
    }

    if ((fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
        ERR("open");
    if (writev(fd, buffers, N) < size)
        ERR("writev");
    if (close(fd) < 0)
        ERR("close");

    free(buffers);
    free(books);
    return EXIT_SUCCESS;
}


//========================================================================================================================
//11) Kopiowanie informacji z plików binarnych używając funkcji mmap
//========================================================================================================================

void copy_file_mmap(const char *src_path, const char *dst_path)
{
    /* 1. Otwarcie pliku źródłowego do odczytu i pobranie jego rozmiaru */
    int fd_src = open(src_path, O_RDONLY);
    if (fd_src == -1)
        ERR("open src");

    struct stat sb;
    if (fstat(fd_src, &sb) == -1)
        ERR("fstat");

    size_t length = sb.st_size;

    /* 2. Otwarcie/utworzenie pliku docelowego z flagą O_RDWR (wymagana przez mmap z PROT_WRITE) */
    int fd_dst = open(dst_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd_dst == -1)
        ERR("open dst");

    /* Obsługa pustego pliku (mmap dla length == 0 zwraca błąd EINVAL) */
    if (length > 0)
    {
        /* 3. Ustawienie rozmiaru pliku docelowego (inaczej zapis poza rozmiar pliku wywoła SIGBUS) */
        if (ftruncate(fd_dst, (off_t)length) == -1)
            ERR("ftruncate");

        /* 4. Zmapowanie obu plików do przestrzeni adresowej procesu */
        void *src_map = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd_src, 0);
        if (src_map == MAP_FAILED)
            ERR("mmap src");

        void *dst_map = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_dst, 0);
        if (dst_map == MAP_FAILED)
            ERR("mmap dst");

        /* 5. Skopiowanie danych bezpośrednio w pamięci */
        memcpy(dst_map, src_map, length);

        /* 6. Synchronizacja zmian na dysk i zwolnienie mapowań */
        if (msync(dst_map, length, MS_SYNC) == -1)
            ERR("msync");

        if (munmap(src_map, length) == -1)
            ERR("munmap src");
        if (munmap(dst_map, length) == -1)
            ERR("munmap dst");
    }

    if (close(fd_src) == -1)
        ERR("close src");
    if (close(fd_dst) == -1)
        ERR("close dst");
}


//========================================================================================================================
//12) Zapisanie informacji z kilku deskryptorów przy użyciu polla do pliku
//========================================================================================================================


void collect_inputs_to_file(const char *const *input_paths, nfds_t count, const char *output_path)
{
    int out_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1)
        ERR("open output");

    struct pollfd *fds = malloc(count * sizeof(struct pollfd));
    if (fds == NULL)
        ERR("malloc");

    nfds_t active = 0; //zwykły ulint który trzyma liczbę deskryptorów
    for (nfds_t i = 0; i < count; i++)
    {
        /* Otwarcie w trybie nieblokującym zapobiega zawieszeniu na read() */
        int fd = open(input_paths[i], O_RDONLY | O_NONBLOCK);
        if (fd == -1)
            ERR("open input");

        fds[i].fd = fd; //zapisujemy deskryptor
        fds[i].events = POLLIN; //ustawiamy maskę bitową zdarzeń na które chcemy czekać
        fds[i].revents = 0; //zerujemy return events, tutaj później poll będzie wpisywać flagi
        active++;
    }

    char buf[256];
    while (active > 0)
    {
        /* Oczekiwanie bez limitu czasu (-1) aż którykolwiek deskryptor będzie gotowy */
        int ready = poll(fds, count, -1); //-1 - błąd systemowy, 0 - timeout bez żadnego zdarzenia, 0 > - ilość deskrypotrów dla których zaszło zdarzenie
        if (ready == -1)
            ERR("poll");

        for (nfds_t i = 0; i < count; i++)
        {
            if (fds[i].fd < 0 || fds[i].revents == 0) //fds[i].fd < 0 - czy nie jest wyłączony, fds[i].revents == 0 - czy zaszło zdarzenie
                continue;

            if (fds[i].revents & POLLIN) //fds[i].revents to maska bitowa i bitowy & sprawdza tylko czy na danym bicie jest ustawiony POLLIN, mogą być inne flagi
            {
                while (1)
                {
                    ssize_t n = read(fds[i].fd, buf, sizeof(buf)); //czytamy z deskryptora i zapisujemy do pliku, n - liczba bitów odczytanych
                    if (n > 0)
                    {
                        if (write(out_fd, buf, (size_t)n) == -1)
                            ERR("write");
                    }
                    else if (n == 0)
                    {
                        /* Koniec pliku (EOF) - zamykamy i wyłączamy deskryptor z poll */
                        if (close(fds[i].fd) == -1)
                            ERR("close");
                        fds[i].fd = -1; //wyłączamy deskryptor
                        active--;
                        break;
                    }
                    else
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break; /* Odczytano wszystkie aktualnie dostępne dane */
                        ERR("read");
                    }
                }
            }
            else if (fds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) //POLLHUP - FIFO się zamknęło, POLLERR - wystąpił błąd, POLLNVAL - invalid request, deskryptor jest nieprawidłowy lub nie jest otwarty
            {
                if (close(fds[i].fd) == -1)
                    ERR("close");
                fds[i].fd = -1;
                active--;
            }
        }
    }

    free(fds);
    if (close(out_fd) == -1)
        ERR("close output");
}


//========================================================================================================================
//13) Tworzenie x fifo
//========================================================================================================================


void create_x_fifos(const char *dir_path, int x, mode_t perms)
{
    char fifo_path[1024];

    for (int i = 0; i < x; i++)
    {
        int n;
        if (dir_path != NULL && dir_path[0] != '\0')
            n = snprintf(fifo_path, sizeof(fifo_path), "%s/fifo_%d", dir_path, i + 1);
        else
            n = snprintf(fifo_path, sizeof(fifo_path), "fifo_%d", i + 1);

        if (n < 0 || (size_t)n >= sizeof(fifo_path))
            ERR("snprintf");

        if (mkfifo(fifo_path, perms) == -1)
            ERR("mkfifo");
    }
}


//========================================================================================================================
//14.1) Zad 2.1 etap 1, Weryfikacja rozmiaru pliku używając lseek i tworzenie pliku wypełnionego zerami, jedno fifo otwarte
//========================================================================================================================

void whatever(){

/* Otwiera istniejący plik przepisu (weryfikując 2048 B) lub tworzy nowy wypełniony spacjami */
int open_or_create_recipe(const char *path)
{
    int fd = open(path, O_RDWR);
    if (fd != -1)
    {
        off_t size = lseek(fd, 0, SEEK_END); //przesunięcie kursora na koniec pliku i zapisanie ilości bajtów, off_t - zmienna dla rozmiarów i przesunięć w plikach
        if (size == -1)
            ERR("lseek");

        if (size != RECIPE_SIZE)
        {
            fprintf(stderr, "Ta ksiega to nie przepis!\n");
            close(fd);
            exit(EXIT_FAILURE);
        }

        if (lseek(fd, 0, SEEK_SET) == -1) //cofamy się na początek
            ERR("lseek");

        return fd;
    }

    if (errno != ENOENT)
        ERR("open");

    fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd == -1)
        ERR("open");

    char empty_line[LINE_LEN];
    memset(empty_line, ' ', LINE_LEN - 1); //ustawiamy wszystkie bajty na spacje
    empty_line[LINE_LEN - 1] = '\n';

    for (int i = 0; i < RECIPE_LINES; i++)
    {
        if (write(fd, empty_line, LINE_LEN) != LINE_LEN)
            ERR("write");
    }

    if (lseek(fd, 0, SEEK_SET) == -1)
        ERR("lseek");

    return fd;
}

/* Weryfikuje, czy ścieżka wskazuje na FIFO i otwiera ją do odczytu */
int open_tower_fifo(const char *path)
{
    struct stat sb;
    if (stat(path, &sb) == -1 || !S_ISFIFO(sb.st_mode)) //jeżeli plik nie istnieje/nie mamy uprawnień LUB nie jest to fifo
    {
        fprintf(stderr, "Nie ma babci na wieży %s\n", path);
        exit(EXIT_FAILURE);
    }

    int fd = open(path, O_RDONLY);
    if (fd == -1) // jeżeli nie udało się otworzyć
    {
        fprintf(stderr, "Nie ma babci na wieży %s\n", path);
        exit(EXIT_FAILURE);
    }

    return fd;
}

int main(int argc, char **argv)
{
    if (argc < 3 || argc > 7)
        usage(argv[0]);

    int recipe_fd = open_or_create_recipe(argv[1]);
    int tower_fd = open_tower_fifo(argv[2]);

    char buf[256];
    while (1)
    {
        ssize_t n = read(tower_fd, buf, sizeof(buf));
        if (n == -1)
            ERR("read");
        if (n == 0)
            break;

        if (write(STDOUT_FILENO, buf, (size_t)n) == -1)
            ERR("write");
    }

    if (close(tower_fd) == -1)
        ERR("close");
    if (close(recipe_fd) == -1)
        ERR("close");

    return EXIT_SUCCESS;
}

}
//========================================================================================================================
//14.2) Zad 2.1 etap 2 parsowanie wiadomości na kilka razy
//========================================================================================================================

void whatever(){
int open_or_create_recipe(const char *path)
{
    //Funkcja ta sama co w poprzednim etapie
    return 0;
}

int open_tower_fifo(const char *path)
{
    //Funkcja ta sama co w poprzednim etapie
   return 0;
}

/* Parsuje wiadomość (bez '\n') i zapisuje poprawny krok w linii NN pliku przepisu */
void process_message(int recipe_fd, const char *msg)
{
    const char *colon = strchr(msg, ':');
    if (colon == NULL || colon == msg) //jeżeli wiadomość jest błędna
    {
        fprintf(stderr, "Babcia się zagadała: %s\n", msg);
        return;
    }

    /* Weryfikacja, czy przed dwukropkiem są wyłącznie cyfry */
    for (const char *p = msg; p < colon; p++)
    {
        if (!isdigit((unsigned char)*p))
        {
            fprintf(stderr, "Babcia się zagadała: %s\n", msg);
            return;
        }
    }

    char *endptr = NULL; //miejsce w którym strtol zakończyłdziałanie
    errno = 0;
    long step = strtol(msg, &endptr, 10); //10 - system liczbowy
    if (errno != 0 || endptr != colon || step < 0 || step >= RECIPE_LINES)
    {
        fprintf(stderr, "Babcia się zagadała: %s\n", msg);
        return;
    }

    /* Przygotowanie 64-bajtowej linii: 63 znaki treści dopełnione spacjami + '\n' */
    const char *content = colon + 1;
    size_t content_len = strlen(content);
    if (content_len > LINE_LEN - 1)
        content_len = LINE_LEN - 1;

    char formatted_line[LINE_LEN];
    memset(formatted_line, ' ', LINE_LEN - 1);
    memcpy(formatted_line, content, content_len);
    formatted_line[LINE_LEN - 1] = '\n';

    if (lseek(recipe_fd, (off_t)(step * LINE_LEN), SEEK_SET) == -1) //ustawia kursor na początek lini step
        ERR("lseek");

    if (write(recipe_fd, formatted_line, LINE_LEN) != LINE_LEN)
        ERR("write");

    printf("Krok %02ld zapisany\n", step);
}

int main(int argc, char **argv)
{
    if (argc < 3 || argc > 7) //7 = nazwa pliku + recipe + 5 wież
        usage(argv[0]);

    int recipe_fd = open_or_create_recipe(argv[1]);
    int tower_fd = open_tower_fifo(argv[2]);

    char line_buf[MSG_BUF_SIZE];
    size_t buf_used = 0;
    char read_buf[128];

    while (1)
    {
        ssize_t n = read(tower_fd, read_buf, sizeof(read_buf));
        if (n == -1)
            ERR("read");
        if (n == 0)
            break;

        for (ssize_t i = 0; i < n; i++)
        {
            if (read_buf[i] == '\n')
            {
                line_buf[buf_used] = '\0';
                process_message(recipe_fd, line_buf);
                buf_used = 0;
            }
            else if (buf_used < sizeof(line_buf) - 1)
            {
                line_buf[buf_used++] = read_buf[i];
            }
        }
    }

    /* Obsługa ewentualnej niedokończonej wiadomości bez '\n' przed EOF */
    if (buf_used > 0)
    {
        line_buf[buf_used] = '\0';
        process_message(recipe_fd, line_buf);
    }

    if (close(tower_fd) == -1)
        ERR("close");
    if (close(recipe_fd) == -1)
        ERR("close");

    return EXIT_SUCCESS;
}

}

//========================================================================================================================
//14.3) Zad 2.1 etap 3 Kilka deskryptorów przy użyciu poll
//========================================================================================================================

void whatever(){
typedef struct
{
    const char *path;
    char line_buf[MSG_BUF_SIZE];
    size_t buf_used;
} TowerState;


int open_or_create_recipe(const char *path)
{
    // Funkcja ta sama co w poprzednim etapie
    return 0;
}

/* ZMIANA W ETAPIE 3: otwarcie z flagą O_NONBLOCK */
int open_tower_fifo(const char *path)
{
    struct stat sb;
    if (stat(path, &sb) == -1 || !S_ISFIFO(sb.st_mode))
    {
        fprintf(stderr, "Nie ma babci na wieży %s\n", path);
        exit(EXIT_FAILURE);
    }

    int fd = open(path, O_RDONLY | O_NONBLOCK); //dodano nonblock
    if (fd == -1)
    {
        fprintf(stderr, "Nie ma babci na wieży %s\n", path);
        exit(EXIT_FAILURE);
    }

    return fd;
}


void process_message(int recipe_fd, const char *msg)
{
    //to samo co w poprzednim etapie
}

/* NOWA FUNKCJA W ETAPIE 3: sprzątanie po zakończeniu transmisji z danej wieży */
void close_tower(int recipe_fd, struct pollfd *pfd, TowerState *tower, int *active)
{
    if (tower->buf_used > 0)
    {
        tower->line_buf[tower->buf_used] = '\0';
        process_message(recipe_fd, tower->line_buf);
        tower->buf_used = 0;
    }

    if (close(pfd->fd) == -1)
        ERR("close");

    printf("Babcia z wieży %s skończyła dyktować\n", tower->path);
    pfd->fd = -1;
    (*active)--;
}

int main(int argc, char **argv)
{
    if (argc < 3 || argc > 7) // 7 = nazwa pliku + recipe + 5 wież
        usage(argv[0]);

    int recipe_fd = open_or_create_recipe(argv[1]);

    nfds_t num_towers = (nfds_t)(argc - 2); //nfds_t - parametr służący do reprezentowania liczby deskryptorów
    struct pollfd fds[MAX_TOWERS];
    TowerState towers[MAX_TOWERS];

    /* Otwarcie wszystkich podanych łączy i inicjalizacja struktur dla poll */
    for (nfds_t i = 0; i < num_towers; i++)
    {
        towers[i].path = argv[i + 2];
        towers[i].buf_used = 0;

        fds[i].fd = open_tower_fifo(towers[i].path);
        fds[i].events = POLLIN;
        fds[i].revents = 0;
    }

    int active = (int)num_towers;
    char read_buf[128];

    while (active > 0)
    {
        int ready = poll(fds, num_towers, 3000);
        if (ready == -1)
        {
            if (errno == EINTR)
                continue;
            ERR("poll");
        }

        if (ready == 0)
        {
            printf("Babcie dolewają herbatki...\n");
            continue;
        }

        for (nfds_t i = 0; i < num_towers; i++)
        {
            if (fds[i].fd < 0 || fds[i].revents == 0)
                continue;

            if (fds[i].revents & (POLLIN | POLLHUP))
            {
                while (1)
                {
                    ssize_t n = read(fds[i].fd, read_buf, sizeof(read_buf));
                    if (n > 0)
                    {
                        for (ssize_t j = 0; j < n; j++)
                        {
                            if (read_buf[j] == '\n') //jeśli zakończyliśmy wiadomość od babci
                            {
                                towers[i].line_buf[towers[i].buf_used] = '\0';
                                process_message(recipe_fd, towers[i].line_buf);
                                towers[i].buf_used = 0;
                            }
                            else if (towers[i].buf_used < sizeof(towers[i].line_buf) - 1) //jeśli mamy miejsce w buforze to dopisujemy znak
                            {
                                towers[i].line_buf[towers[i].buf_used++] = read_buf[j];
                            }
                        }
                    }
                    else if (n == 0) //koniec deskryptora
                    {
                        close_tower(recipe_fd, &fds[i], &towers[i], &active);
                        break;
                    }
                    else //inne dziwne rzeczy
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        ERR("read");
                    }
                }
            }
            else if (fds[i].revents & (POLLERR | POLLNVAL)) //errory
            {
                close_tower(recipe_fd, &fds[i], &towers[i], &active);
            }
        }
    }

    if (close(recipe_fd) == -1)
        ERR("close");

    return EXIT_SUCCESS;
}

}

//========================================================================================================================
//14.4) Zad 2.1 etap 4 Użycie mmap oraz zamykanie fifo
//========================================================================================================================
void whatever{

//takie samo jak poprzednio
typedef struct
{
    const char *path;
    char line_buf[MSG_BUF_SIZE];
    size_t buf_used;
} TowerState;

//takie samo jak poprzednio
void usage(const char *pname)
{
    fprintf(stderr, "USAGE: %s recipe tower1 [tower2 ... tower5]\n", pname);
    exit(EXIT_FAILURE);
}

//takie samo jak poprzednio
int open_or_create_recipe(const char *path)
{
    int fd = open(path, O_RDWR);
    if (fd != -1)
    {
        off_t size = lseek(fd, 0, SEEK_END);
        if (size == -1)
            ERR("lseek");

        if (size != RECIPE_SIZE)
        {
            fprintf(stderr, "Ta ksiega to nie przepis!\n");
            close(fd);
            exit(EXIT_FAILURE);
        }

        if (lseek(fd, 0, SEEK_SET) == -1)
            ERR("lseek");

        return fd;
    }

    if (errno != ENOENT)
        ERR("open");

    fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd == -1)
        ERR("open");

    char empty_line[LINE_LEN];
    memset(empty_line, ' ', LINE_LEN - 1);
    empty_line[LINE_LEN - 1] = '\n';

    for (int i = 0; i < RECIPE_LINES; i++)
    {
        if (write(fd, empty_line, LINE_LEN) != LINE_LEN)
            ERR("write");
    }

    if (lseek(fd, 0, SEEK_SET) == -1)
        ERR("lseek");

    return fd;
}

//takie samo jak poprzednio
int open_tower_fifo(const char *path)
{
    struct stat sb;
    if (stat(path, &sb) == -1 || !S_ISFIFO(sb.st_mode))
    {
        fprintf(stderr, "Nie ma babci na wieży %s\n", path);
        exit(EXIT_FAILURE);
    }

    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd == -1)
    {
        fprintf(stderr, "Nie ma babci na wieży %s\n", path);
        exit(EXIT_FAILURE);
    }

    return fd;
}

//takie samo jak poprzednio
void process_message(int recipe_fd, const char *msg)
{
    const char *colon = strchr(msg, ':');
    if (colon == NULL || colon == msg)
    {
        fprintf(stderr, "Babcia się zagadała: %s\n", msg);
        return;
    }

    for (const char *p = msg; p < colon; p++)
    {
        if (!isdigit((unsigned char)*p))
        {
            fprintf(stderr, "Babcia się zagadała: %s\n", msg);
            return;
        }
    }

    char *endptr = NULL;
    errno = 0;
    long step = strtol(msg, &endptr, 10);
    if (errno != 0 || endptr != colon || step < 0 || step >= RECIPE_LINES)
    {
        fprintf(stderr, "Babcia się zagadała: %s\n", msg);
        return;
    }

    const char *content = colon + 1;
    size_t content_len = strlen(content);
    if (content_len > LINE_LEN - 1)
        content_len = LINE_LEN - 1;

    char formatted_line[LINE_LEN];
    memset(formatted_line, ' ', LINE_LEN - 1);
    memcpy(formatted_line, content, content_len);
    formatted_line[LINE_LEN - 1] = '\n';

    if (lseek(recipe_fd, (off_t)(step * LINE_LEN), SEEK_SET) == -1)
        ERR("lseek");

    if (write(recipe_fd, formatted_line, LINE_LEN) != LINE_LEN)
        ERR("write");

    printf("Krok %02ld zapisany\n", step);
}

//takie samo jak poprzednio
void close_tower(int recipe_fd, struct pollfd *pfd, TowerState *tower, int *active)
{
    if (tower->buf_used > 0)
    {
        tower->line_buf[tower->buf_used] = '\0';
        process_message(recipe_fd, tower->line_buf);
        tower->buf_used = 0;
    }

    if (close(pfd->fd) == -1)
        ERR("close");

    printf("Babcia z wieży %s skończyła dyktować\n", tower->path);
    pfd->fd = -1;
    (*active)--;
}

/* NOWA FUNKCJA W ETAPIE 4: mapowanie pliku przepisu przez mmap, czyszczenie i raport końcowy */
void finalize_recipe_mmap(int recipe_fd)
{
    char *map = mmap(NULL, RECIPE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, recipe_fd, 0); //NULL - system sam wybiera adres gdzie mapujemy, MAP_SHARED - zmiany są wprowadzane również na dysku
    if (map == MAP_FAILED)
        ERR("mmap");

    const char *dirty = "raw~berries";
    const char *clean = "raspberries";
    const size_t word_len = 11; /* Oba słowa mają dokładnie 11 znaków */

    int cleaned_count = 0;
    for (int i = 0; i < RECIPE_LINES; i++)
    {
        char *line = map + i * LINE_LEN;
        /* Szukamy wystąpień w obrębie 63 znaków treści (bez '\n') */
        for (size_t pos = 0; pos <= (LINE_LEN - 1) - word_len; pos++)
        {
            if (memcmp(line + pos, dirty, word_len) == 0) //sprawdzamy czy trzeba czyścić
            {
                memcpy(line + pos, clean, word_len);
                cleaned_count++;
                pos += word_len - 1;
            }
        }
    }

    printf("Brudy księżniczki Joanny wyczyszczone: %d\n", cleaned_count);

    int empty_steps = 0;
    for (int i = 0; i < RECIPE_LINES; i++)
    {
        const char *line = map + i * LINE_LEN; //ustawiamy wskaźnik na początek i-tej liniu

        /* Szukamy ostatniego znaku niebędącego spacją przed '\n' */
        int last_non_space = LINE_LEN - 2;
        while (last_non_space >= 0 && line[last_non_space] == ' ')
            last_non_space--;

        if (last_non_space < 0)
        {
            empty_steps++;
        }
        else
        {
            int content_len = last_non_space + 1;
            printf("%02d: %.*s\n", i, content_len, line);
        }
    }

    if (empty_steps > 0)
        printf("Brakuje %d kroków, tort sie zapadnie!\n", empty_steps);
    else
        printf("Tort gotowy!\n");

    if (msync(map, RECIPE_SIZE, MS_SYNC) == -1) //msync - synchronizacja pamięci, MS_SYNC - flaga wstrzymuje działanie programu do momentu, aż system operacyjny fizycznie zapisze wszystkie modyfikacje na dysk
        ERR("msync");
    if (munmap(map, RECIPE_SIZE) == -1) //coś jak free tylko dla pamięci z map
        ERR("munmap");
}

int main(int argc, char **argv)
{
    if (argc < 3 || argc > 7) // 7 = nazwa pliku + recipe + 5 wież
        usage(argv[0]);

    int recipe_fd = open_or_create_recipe(argv[1]);

    nfds_t num_towers = (nfds_t)(argc - 2); //nfds_t - parametr służący do reprezentowania liczby deskryptorów
    struct pollfd fds[MAX_TOWERS];
    TowerState towers[MAX_TOWERS];

    for (nfds_t i = 0; i < num_towers; i++)
    {
        towers[i].path = argv[i + 2];
        towers[i].buf_used = 0;

        fds[i].fd = open_tower_fifo(towers[i].path);
        fds[i].events = POLLIN;
        fds[i].revents = 0;
    }

    int active = (int)num_towers;
    char read_buf[128];

    while (active > 0)
    {
        int ready = poll(fds, num_towers, 3000);
        if (ready == -1)
        {
            if (errno == EINTR)
                continue;
            ERR("poll");
        }

        if (ready == 0)
        {
            printf("Babcie dolewają herbatki...\n");
            continue;
        }

        for (nfds_t i = 0; i < num_towers; i++)
        {
            if (fds[i].fd < 0 || fds[i].revents == 0)
                continue;

            if (fds[i].revents & (POLLIN | POLLHUP))
            {
                while (1)
                {
                    ssize_t n = read(fds[i].fd, read_buf, sizeof(read_buf));
                    if (n > 0)
                    {
                        for (ssize_t j = 0; j < n; j++)
                        {
                            if (read_buf[j] == '\n') //jeśli zakończyliśmy wiadomość od babci
                            {
                                towers[i].line_buf[towers[i].buf_used] = '\0';
                                process_message(recipe_fd, towers[i].line_buf);
                                towers[i].buf_used = 0;
                            }
                            else if (towers[i].buf_used < sizeof(towers[i].line_buf) - 1) //jeśli mamy miejsce w buforze to dopisujemy znak
                            {
                                towers[i].line_buf[towers[i].buf_used++] = read_buf[j];
                            }
                        }
                    }
                    else if (n == 0) //koniec deskryptora
                    {
                        close_tower(recipe_fd, &fds[i], &towers[i], &active);
                        break;
                    }
                    else //inne dziwne rzeczy
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        ERR("read");
                    }
                }
            }
            else if (fds[i].revents & (POLLERR | POLLNVAL)) //errory
            {
                close_tower(recipe_fd, &fds[i], &towers[i], &active);
            }
        }
    }

    /* Etap 4: przetworzenie gotowego pliku w pamięci po zamknięciu wszystkich wież */
    finalize_recipe_mmap(recipe_fd);

    if (close(recipe_fd) == -1)
        ERR("close");

    return EXIT_SUCCESS;
}


}
