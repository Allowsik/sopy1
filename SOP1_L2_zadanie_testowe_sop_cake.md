# SOP1 L2 · Zadanie testowe „Tort” (`sop-cake`)

> **Samodzielna ściąga offline:** kompletne rozwiązanie C + wyjaśnienie KAŻDEGO mechanizmu + krótkie fragmenty do przerabiania na innych zadaniach.  
> **Jak korzystać na labach:** `Ctrl+F` → wpisz `POTRZEBUJĘ:` albo nazwę funkcji (`poll`, `pwrite`, `mmap`, `EINTR`).  
> **Status:** rozwiązanie poniżej jest **autorskie**, nie jest oficjalnym wzorcowym rozwiązaniem uczelni. Kod został skompilowany `gcc -std=c11 -Wall -Wextra -Wpedantic` i przetestowany na kilku FIFO.

**Oryginalna treść:** [zadanie testowe nr 1, SOP1 L2, MiNI PW](https://sop.mini.pw.edu.pl/pl/sop1/lab/l2/example1/) ([plik źródłowy uczelni na GitHub](https://github.com/SOP-MINI/sop-site/blob/master/content/sop1/lab/l2/example1.pl.md)). Wymagania niżej są skrótowym omówieniem oryginału, natomiast kod i objaśnienia napisałem osobno.

---

## ⚡ START: co gdzie znaleźć w 10 sekund

| POTRZEBUJĘ... | Skocz do | Najważniejsze |
|---|---|---|
| Stworzyć plik i uzupełnić go spacjami | [Etap 1](#etap-1-plik-przepisu--2048-bajtów) | `open(...O_CREAT\|O_EXCL...)`, `write_all()` |
| Sprawdzić dokładny rozmiar pliku | [Etap 1](#etap-1-plik-przepisu--2048-bajtów) | `lseek(fd, 0, SEEK_END)` |
| Sprawdzić, czy ścieżka jest FIFO | [FIFO](#potrzebuję-sprawdzić-czy-to-fifo) | `stat()` + `S_ISFIFO()` |
| Czytać aż do EOF, bez ucięcia danych | [Odczyt](#potrzebuję-czytać-aż-do-eof) | `read()` w pętli; `0` = EOF |
| Wypisać **wszystkie** bajty, nawet po short write | [write_all](#potrzebuję-zapisać-wszystkie-bajty) | `done += n` z **write**, nie z read |
| Składać linie, gdy `read()` zwraca kawałki | [Etap 2](#etap-2-wiadomości-nntreść-i-zapis-kroku) | bufor + licznik + `\n` |
| Zapisać w konkretnej linii pliku | [pwrite](#potrzebuję-zapisać-rekord-numer-n) | `pwrite(fd, record, 64, NN*64)` + retry |
| Czytać **kilka FIFO naraz** | [Etap 3](#etap-3-kilka-fifo-naraz-poll) | `poll()` + `POLLIN` + `POLLHUP` |
| Czekać bez busy waiting | [Etap 3](#etap-3-kilka-fifo-naraz-poll) | `poll(fds, count, 3000)` |
| Zmienić zawartość pliku jak tablicę | [Etap 4](#etap-4-mmap--poprawianie-tekstu-i-wydruk) | `mmap()` → `p[i] = ...` → `msync()` → `munmap()` |
| Zobaczyć **cały** program | [Pełny kod](#pełny-kod-sop-cakec) | gotowy `sop-cake.c` |
| Nie działa / dziwne błędy | [Awaryjna diagnostyka](#awaryjna-diagnostyka--zobacz-zanim-spanikujesz) | EOF, EINTR, EAGAIN, brak piszącego FIFO |
| Uruchomić bez uczelnianego `granny.sh` | [Test ręczny](#test-ręczny-bez-grannysh) | `mkfifo` i `printf > tower1` |

## 0. Co dokładnie muszę zbudować?

Uruchomienie:

```bash
./sop-cake recipe.txt tower1 [tower2 ... tower5]
```

- `recipe.txt`: 32 rekordy po 64 bajty, razem **2048 bajtów**. Każdy rekord ma **63 bajty treści/spacji i 1 znak `\n`**.
- `tower1` ... `tower5`: od 1 do 5 **FIFO**, czyli łączy nazwanych, z których babcie wysyłają tekst `NN:treść\n`.
- `NN`: numer wiersza, liczba od **0 do 31**; treść może mieć maksymalnie 63 bajty w pliku.
- Jeden `read()` może oddać **pół wiadomości**, kilka wiadomości naraz albo tylko kawałek. `read()` nie obiecuje granic linii.
- **Nie wolno busy waiting**: przy kilku FIFO użyj `poll()` zamiast pętli nieustannie próbującej czytać.

**Etapy oceniania z oryginału:** (1) plik + jedno FIFO i surowy wydruk, (2) parsowanie i zapis kroku, (3) wiele FIFO z `poll`, (4) `mmap` + poprawki + raport.

### Obraz w głowie

```text
granny.sh → FIFO tower1 ───┐
granny.sh → FIFO tower2 ───┼──► poll() ─► read() ─► składanie linii po \n
                             │                           │
                             └───────────────────────────► NN:treść
                                                         │
                                   pwrite(recipe, NN*64, rekord 64B)
                                                         │
                                           po zakończeniu wszystkich FIFO
                                                         ▼
                                      mmap(recipe) → zamiana literówek
                                                         │
                                           wydruk kroków → munmap/close
```

---

## Etap 1. Plik przepisu = 2048 bajtów

**Pytanie:** skąd wziąć 2048 bajtów? **32 × (63 spacje + 1 `\n`)**.

### POTRZEBUJĘ: utworzyć plik, ale nie nadpisać istniejącego

```c
int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0644);
if (fd < 0 && errno == EEXIST) {
    fd = open(path, O_RDWR); // istniał; tylko otwórz
}
if (fd < 0) perror("open");
```

- `O_RDWR`: potrzebne do odczytu i modyfikacji, również w etapie `mmap`.
- `O_CREAT`: utwórz brakujący plik.
- `O_EXCL`: w parze z `O_CREAT` **odmów**, jeżeli plik już istnieje. Dzięki temu rozróżniasz nowy/stary.
- `0644`: uprawnienia dla **nowo utworzonego** pliku (ostatecznie ogranicza je też `umask`).
- `errno == EEXIST`: to oczekiwany przypadek istniejącego przepisu, nie awaria.

### POTRZEBUJĘ: zbudować 32 puste linie

```c
char book[2048];
for (int i = 0; i < 32; ++i) {
    memset(book + i * 64, ' ', 63); // bajty od i*64 do i*64+62
    book[i * 64 + 63] = '\n';       // 64. bajt rekordu
}
write_all(fd, book, sizeof(book));
```

**Co robi `book + i * 64`?** To wskaźnik na początek rekordu `i`. Rekord 7 zaczyna się od bajtu **448**, rekord 31 od bajtu **1984**. `memset` wypełnia spacje, `\n` ustawia koniec linii.

### POTRZEBUJĘ: sprawdzić rozmiar istniejącego pliku przez `lseek`

```c
off_t size = lseek(fd, 0, SEEK_END);
if (size != 2048) {
    fprintf(stderr, "Ta ksiega to nie przepis!\n");
    // zamknij fd i zakończ program
}
```

`SEEK_END` oznacza: ustaw pozycję na końcu pliku. Zwrócona **pozycja** dla zwykłego pliku jest tu jego rozmiarem. Nie pomyl `lseek()` z odczytem: **nie czyta bajtów**. Nie używaj `O_TRUNC`: skasowałoby istniejący przepis.

### POTRZEBUJĘ: sprawdzić, czy to FIFO

```c
struct stat st;
if (stat(path, &st) == -1 || !S_ISFIFO(st.st_mode)) {
    fprintf(stderr, "Nie ma babci na wieży %s\n", path);
    return 1;
}
```

`stat()` pobiera metadane, `S_ISFIFO(st.st_mode)` mówi, czy ścieżka wskazuje na łącze nazwane. To **nie** jest ten sam typ pliku co zwykły `recipe.txt`.

### POTRZEBUJĘ: czytać aż do EOF

**Sam etap 1** wymaga surowego przesłania wiadomości z FIFO na ekran. Ten fragment **zastępujesz etapami 2–4**, kiedy już zaczynasz składać wiadomości i zapisywać kroki:

```c
char buf[256];
for (;;) {
    ssize_t n = read(fifo_fd, buf, sizeof(buf));
    if (n < 0 && errno == EINTR) continue;
    if (n < 0) { perror("read"); break; }
    if (n == 0) break;             // piszący zamknął FIFO → EOF
    write_all(STDOUT_FILENO, buf, (size_t)n);
}
```

**Nie używaj `printf("%s", buf)`** bez dopisanego `\0`: `read()` dostarcza **bajty**, nie gotowy łańcuch C.

### POTRZEBUJĘ: zapisać wszystkie bajty

```c
void write_all(int fd, const char *buf, size_t len) {
    size_t done = 0;
    while (done < len) {
        ssize_t n = write(fd, buf + done, len - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { perror("write"); exit(EXIT_FAILURE); }
        done += (size_t)n; // UWAGA: dodajemy wynik WRITE, nie READ
    }
}
```

**Przykład:** masz 100 bajtów, pierwsze `write()` wypisało 30 → `done=30`, kolejne `write()` bierze `buf+30` i `100-30=70` bajtów. To eliminuje **short writes**. Dla `write()==0` najlepiej wypisać własny komunikat (to nie musi ustawiać `errno`); pełny kod niżej obsługuje to osobno.

---

## Etap 2. Wiadomości `NN:treść` i zapis kroku

### Problem, który musisz zrozumieć

Babcia mówi `07:Wymieszaj ciasto\n`, ale `read()` może zwrócić:

```text
read nr 1: "07:Wymie"
read nr 2: "szaj "
read nr 3: "ciasto\n03:"
read nr 4: "Wstaw do pieca\n"
```

**Nie można parsować pojedynczego `read()` jako wiadomości.** Zbierasz bajty do **bufora linii**, aż trafisz na `\n`. Każda kompletna linia jest osobną wiadomością.

### POTRZEBUJĘ: składać wiadomości po `\n`

```c
char line[128];
size_t used = 0;

// Ten kod wykonuj dla KAŻDEGO bajtu odczytanego przez read():
if (c == '\n') {
    line[used] = '\0';       // teraz to poprawny napis C
    // process_line(line);  // waliduj NN:treść i zapisuj krok
    used = 0;               // zaczynamy kolejną wiadomość
} else if (used < 127) {
    line[used++] = c;
} else {
    // za długa wiadomość; pomiń do najbliższego \n
}
```

**Istotne:** w pełnym rozwiązaniu poniżej każdy `Tower` ma **osobne** `line`, `used` i flagę zbyt długiej wiadomości. Tego potrzeba w etapie 3, bo odczyty z kilku FIFO się przeplatają.

### POTRZEBUJĘ: sprawdzić `NN:treść`

```c
// Bezpośrednio w pełnym rozwiązaniu: process_line()
// 1. Czytaj kolejne cyfry na początku wiadomości.
// 2. Przekształcaj na numer: number = number * 10 + digit.
// 3. Sprawdź 0 <= number <= 31.
// 4. Pierwszy znak po cyfrach MUSI być ':'.
// 5. Wszystko po ':' to treść (może też zawierać ':').
```

Nie używaj `atoi("banan")`: zwróci 0 i pomyli niepoprawną wiadomość z krokiem nr 0. Dla bardziej ogólnych liczb sprawdza się `strtol` wraz z kontrolą `endptr`; tu ręczne czytanie cyfr jest czytelne.

### POTRZEBUJĘ: zapisać rekord numer N

```c
char record[64];
memset(record, ' ', 63);
record[63] = '\n';
size_t len = strlen(text);
if (len > 63) len = 63;
memcpy(record, text, len);

// Jeden zapis w odpowiednie miejsce, ale w praktyce poprawnie
// obsłuż częściowe zapisy i EINTR pętlą (pełny kod: save_step).
pwrite(recipe_fd, record, 64, (off_t)number * 64);
```

`pwrite(fd, bufor, liczba_bajtów, pozycja)` to **zapis pod konkretny offset**, który nie przesuwa bieżącej pozycji pliku. W zadaniu z `NN=07`: `7*64=448`. Dlaczego **nie `strcpy(record, text)`**? Potrzebujesz dokładnie 64 bajtów, spacji i `\n`, a nie kończącego `\0`.

**Gdy polecenie każe obowiązkowo używać `lseek()+write()`:**

```c
if (lseek(recipe_fd, (off_t)number * 64, SEEK_SET) == (off_t)-1)
    perror("lseek");
write_all(recipe_fd, record, sizeof(record));
```

Uwaga: `lseek()+write()` ma wspólny offset pliku i nie jest atomową parą operacji. W tym zadaniu wszystko zapisuje jeden proces, więc to wystarcza. `pwrite()` jest wygodniejsze.

---

## Etap 3. Kilka FIFO naraz: `poll()`

### Najważniejsza idea

Nie możesz zrobić: „czytam tower1 do końca, potem tower2”, bo tower1 może akurat robić długą przerwę, a tower2 już ma dane. **`poll()` czeka na dane w dowolnym ze wskazanych deskryptorów.** To nie jest busy waiting.

### POTRZEBUJĘ: uruchomić `poll()` na 2 FIFO

```c
struct pollfd fds[2] = {
    {.fd = fifo1, .events = POLLIN | POLLHUP},
    {.fd = fifo2, .events = POLLIN | POLLHUP}
};

int result = poll(fds, 2, 3000); // 3000 ms = 3 sekundy
if (result == -1 && errno == EINTR) {
    // ponów poll w swojej pętli
} else if (result == -1) {
    perror("poll");
} else if (result == 0) {
    // żadnych zdarzeń przez 3 sekundy
} else {
    // dla każdego fds[i] sprawdź fds[i].revents
}
```

`events` = czego chcę nasłuchiwać, `revents` = co **faktycznie** się wydarzyło.

| Flaga/rezultat | Znaczenie | Co zrobić |
|---|---|---|
| `POLLIN` | Można czytać | `read(fd,...)`, poskładaj linie |
| `POLLHUP` | Druga strona zamknęła łącze | Nadal `read()` (mogą zostać bajty), dopiero `read()==0` oznacza EOF |
| `POLLERR` | Błąd zgłoszony dla deskryptora | Sprawdź `read`, rozpoznaj sytuację |
| `POLLNVAL` | Niepoprawny deskryptor | Błąd programu lub fd już zamknięty |
| `poll()==0` | Timeout | Wypisz „Babcie dolewają herbatki...” |
| `poll()==-1` + `EINTR` | Sygnał przerwał czekanie | Wróć do `poll()` |

### POTRZEBUJĘ: wyłączyć zamknięte FIFO z `poll()`

```c
if (read_result == 0) {
    close(fds[i].fd);
    fds[i].fd = -1;    // poll ignoruje wpisy z fd < 0
    active--;          // kończymy, gdy active == 0
}
```

**Prawidłowa kolejność:** `poll` wykrywa zdarzenie → `read` → `consume` składa pełne linie → jeśli `read==0`, zamknij i wyłącz deskryptor. Sam `POLLHUP` nie oznacza, że **w buforze jądra nie ma już żadnych bajtów**.

**Najczęstsza pułapka:** jeden wspólny `line[]` dla wielu FIFO. Wtedy otrzymasz np. początek wiadomości z wieży 1 sklejony z końcem z wieży 2. W pełnym kodzie używamy `struct Tower`.

---

## Etap 4. `mmap()` – poprawianie tekstu i wydruk

### POTRZEBUJĘ: zmieniać plik przez wskaźnik

```c
char *p = mmap(NULL, 2048, PROT_READ | PROT_WRITE,
               MAP_SHARED, recipe_fd, 0);
if (p == MAP_FAILED) perror("mmap");
// tutaj p[i] czyta/modyfikuje bajty pliku
if (msync(p, 2048, MS_SYNC) == -1) perror("msync");
if (munmap(p, 2048) == -1) perror("munmap");
```

- `NULL`: system wybiera adres mapowania.
- `2048`: długość w bajtach.
- `PROT_READ | PROT_WRITE`: chcemy móc odczytywać i zmieniać bajty.
- `MAP_SHARED`: zmiany są współdzielone z plikiem (z `MAP_PRIVATE` plik by się nie zmienił).
- `recipe_fd`: plik otwarty **`O_RDWR`**.
- `0`: początek pliku.
- `MAP_FAILED`: sposób sprawdzania błędu, **nie `NULL`**.
- `msync(..., MS_SYNC)`: synchronizacja zmian; `munmap` usuwa mapowanie, **nie** zastępuje gwarancji `msync`.

### POTRZEBUJĘ: podmienić WSZYSTKIE wystąpienia tekstu

```c
const char bad[] = "raw~berries";
const char good[] = "raspberries";
size_t changed = 0;
for (size_t i = 0; i + sizeof(bad) - 1 <= 2048; ++i) {
    if (memcmp(p + i, bad, sizeof(bad) - 1) == 0) {
        memcpy(p + i, good, sizeof(good) - 1);
        ++changed;
        i += sizeof(bad) - 2; // pomiń resztę znalezionego fragmentu
    }
}
```

Oba wyrażenia mają **11 znaków**, więc zamiana nie przesuwa kolejnych rekordów. `memcmp` porównuje **surowe bajty**. `memcpy` nadpisuje dokładnie tyle samo bajtów. `sizeof(bad)-1`, bo `sizeof` uwzględnia dodatkowe `\0` w C.

### POTRZEBUJĘ: wydrukować bez końcowych spacji

```c
for (int number = 0; number < 32; ++number) {
    char *line = p + number * 64;
    size_t visible = 63;
    while (visible > 0 && line[visible - 1] == ' ')
        --visible;
    if (visible == 0) {
        // pusty krok
        continue;
    }
    // wydrukuj numer %02d, ': ', potem visible bajtów, potem '\n'
}
```

`p + number*64` to wskaźnik na rekord. `visible` mówi, ile bajtów rzeczywiście wydrukować, bez spacji dopełniających. Linia **nie** jest napisem C zakończonym `\0`, dlatego bezpośrednie `printf("%s", line)` byłoby błędem.

---

## PEŁNY KOD: `sop-cake.c`

**Co otrzymujesz:** jeden, kompletny program realizujący **końcową wersję etapów 2–4**, w tym przygotowanie i walidację z etapu 1. W etapie 1, jeśli chcesz pokazać dokładnie *surowe kopiowanie FIFO na stdout*, zamiast `receive_steps()` użyj osobnego fragmentu „czytać aż do EOF” powyżej; późniejsze etapy celowo zastępują ten wydruk parsowaniem.

Kompilacja:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 sop-cake.c -o sop-cake
```

> Wywołania do obsługi plików, FIFO i ich deskryptorów realizują `open`, `read`, `write`, `pwrite`, `lseek`, `poll`, `stat`, `mmap`, `munmap`, `close` itd. `vsnprintf` tylko formatuje komunikat w **pamięci** przed wysłaniem przez `write_all`; `perror` służy do diagnostyki błędów. Każda funkcja pomocnicza jest opisana komentarzem bezpośrednio przed nią.

```c
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define STEPS 32
#define REC_SIZE 64
#define BOOK_SIZE (STEPS * REC_SIZE)
#define MAX_TOWERS 5
#define MAX_MESSAGE 127

typedef struct {
    const char *path;
    int fd;
    char line[MAX_MESSAGE + 1];
    size_t used;
    int too_long;
} Tower;

static void die(const char *what)
{
    perror(what);
    exit(EXIT_FAILURE);
}

/* write() może zapisać tylko część bufora albo zostać przerwane (EINTR). */
static void write_all(int fd, const void *data, size_t length)
{
    const char *p = data;
    size_t done = 0;
    while (done < length) {
        ssize_t n = write(fd, p + done, length - done);
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0)
            die("write");
        if (n == 0) {
            fprintf(stderr, "write zwrocil 0 - brak postepu\n");
            exit(EXIT_FAILURE);
        }
        done += (size_t)n;
    }
}

/* vsnprintf tylko SKŁADA komunikat w pamięci; write_all go wypisuje. */
static void say(int fd, const char *fmt, ...)
{
    char msg[4096];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    if (n < 0 || (size_t)n >= sizeof(msg)) {
        fprintf(stderr, "Za dlugi lub niepoprawny komunikat\n");
        exit(EXIT_FAILURE);
    }
    write_all(fd, msg, (size_t)n);
}

/* O_CREAT|O_EXCL tworzy nowy plik; EEXIST oznacza, że istnieje. */
static int open_recipe(const char *path)
{
    int fd;
    do {
        fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0644);
    } while (fd < 0 && errno == EINTR);

    if (fd >= 0) {
        char data[BOOK_SIZE];
        for (int i = 0; i < STEPS; ++i) {
            memset(data + i * REC_SIZE, ' ', REC_SIZE - 1);
            data[i * REC_SIZE + REC_SIZE - 1] = '\n';
        }
        write_all(fd, data, sizeof(data));
        return fd;
    }

    if (errno != EEXIST)
        die("open recipe");

    do {
        fd = open(path, O_RDWR);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0)
        die("open existing recipe");

    off_t size = lseek(fd, 0, SEEK_END);
    if (size != (off_t)BOOK_SIZE) {
        say(STDERR_FILENO, "Ta ksiega to nie przepis!\n");
        close(fd);
        exit(EXIT_FAILURE);
    }
    return fd;
}

/* Przepis ma rekordy o stałej długości: indeks 7 -> bajt 7*64. */
static void save_step(int recipe_fd, int number, const char *text, size_t length)
{
    char record[REC_SIZE];
    memset(record, ' ', REC_SIZE - 1);
    record[REC_SIZE - 1] = '\n';

    if (length > REC_SIZE - 1)
        length = REC_SIZE - 1;
    memcpy(record, text, length);

    size_t done = 0;
    off_t pos = (off_t)number * REC_SIZE;
    while (done < sizeof(record)) {
        ssize_t n = pwrite(recipe_fd, record + done,
                           sizeof(record) - done, pos + (off_t)done);
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0)
            die("pwrite");
        if (n == 0) {
            say(STDERR_FILENO, "pwrite nie zapisal zadnych danych\n");
            exit(EXIT_FAILURE);
        }
        done += (size_t)n;
    }
    say(STDOUT_FILENO, "Krok %02d zapisany\n", number);
}

/* Akceptuje cyfry przed ':', a ich wartość musi mieścić się w 0..31. */
static void process_line(int recipe_fd, const char *line, size_t length)
{
    int number = 0;
    size_t i = 0;
    while (i < length && line[i] >= '0' && line[i] <= '9') {
        number = number * 10 + (line[i] - '0');
        if (number >= STEPS)
            break;
        ++i;
    }

    if (i == 0 || i >= length || line[i] != ':' || number >= STEPS) {
        say(STDERR_FILENO, "Babcia się zagadała: %.*s\n", (int)length, line);
        return;
    }
    save_step(recipe_fd, number, line + i + 1, length - i - 1);
}

/* Składa wiadomości znak po znaku: read() nie musi zwrócić całej linii. */
static void consume(Tower *tower, int recipe_fd, const char *data, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        char c = data[i];
        if (c == '\n') {
            if (tower->too_long)
                say(STDERR_FILENO, "Babcia się zagadała: za długa wiadomość\n");
            else
                process_line(recipe_fd, tower->line, tower->used);
            tower->used = 0;
            tower->too_long = 0;
        } else if (!tower->too_long) {
            if (tower->used < MAX_MESSAGE)
                tower->line[tower->used++] = c;
            else
                tower->too_long = 1;
        }
    }
}

/* Najpierw stat(), żeby nie pomylić FIFO ze zwykłym plikiem. */
static void check_tower(const char *path)
{
    struct stat st;
    if (stat(path, &st) < 0 || !S_ISFIFO(st.st_mode)) {
        say(STDERR_FILENO, "Nie ma babci na wieży %s\n", path);
        exit(EXIT_FAILURE);
    }
}

static int open_tower(const char *path)
{
    int fd;
    do {
        fd = open(path, O_RDONLY); /* Może CZEKAĆ na piszącą babcię. */
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) {
        say(STDERR_FILENO, "Nie ma babci na wieży %s\n", path);
        exit(EXIT_FAILURE);
    }
    return fd;
}

static void receive_steps(int recipe_fd, Tower towers[], int count)
{
    struct pollfd watched[MAX_TOWERS];
    for (int i = 0; i < count; ++i) {
        towers[i].fd = open_tower(towers[i].path);
        watched[i].fd = towers[i].fd;
        watched[i].events = POLLIN | POLLHUP;
        watched[i].revents = 0;
    }

    int active = count;
    while (active > 0) {
        int ready = poll(watched, (nfds_t)count, 3000);
        if (ready < 0 && errno == EINTR)
            continue;
        if (ready < 0)
            die("poll");
        if (ready == 0) {
            say(STDOUT_FILENO, "Babcie dolewają herbatki...\n");
            continue;
        }

        for (int i = 0; i < count; ++i) {
            if (watched[i].fd < 0 || watched[i].revents == 0)
                continue;
            if (watched[i].revents & POLLNVAL) {
                say(STDERR_FILENO, "poll: niepoprawny deskryptor\n");
                exit(EXIT_FAILURE);
            }
            if (watched[i].revents & (POLLIN | POLLHUP | POLLERR)) {
                char buf[256];
                ssize_t n = read(watched[i].fd, buf, sizeof(buf));
                if (n < 0 && errno == EINTR)
                    continue;
                if (n < 0)
                    die("read FIFO");
                if (n > 0) {
                    consume(&towers[i], recipe_fd, buf, (size_t)n);
                } else {
                    if (towers[i].used != 0 || towers[i].too_long)
                        say(STDERR_FILENO, "Babcia się zagadała: urwana wiadomość\n");
                    if (close(watched[i].fd) < 0)
                        die("close FIFO");
                    watched[i].fd = -1; /* poll() ignoruje fd < 0. */
                    towers[i].fd = -1;
                    --active;
                    say(STDOUT_FILENO, "Babcia z wieży %s skończyła dyktować\n",
                        towers[i].path);
                }
            }
        }
    }
}

static void finish_recipe(int recipe_fd)
{
    char *map = mmap(NULL, BOOK_SIZE, PROT_READ | PROT_WRITE,
                     MAP_SHARED, recipe_fd, 0);
    if (map == MAP_FAILED)
        die("mmap");

    const char bad[] = "raw~berries";
    const char good[] = "raspberries";
    /* Oba napisy mają taką samą długość - nie przesuwamy rekordów. */
    size_t fixed = 0;
    for (size_t i = 0; i + sizeof(bad) - 1 <= BOOK_SIZE; ++i) {
        if (memcmp(map + i, bad, sizeof(bad) - 1) == 0) {
            memcpy(map + i, good, sizeof(good) - 1);
            ++fixed;
            i += sizeof(bad) - 2;
        }
    }
    if (msync(map, BOOK_SIZE, MS_SYNC) < 0)
        die("msync");
    say(STDOUT_FILENO, "Brudy księżniczki Joanny wyczyszczone: %zu\n", fixed);

    int missing = 0;
    for (int step = 0; step < STEPS; ++step) {
        const char *record = map + (size_t)step * REC_SIZE;
        size_t visible = REC_SIZE - 1;
        while (visible > 0 && record[visible - 1] == ' ')
            --visible;
        if (visible == 0) {
            ++missing;
        } else {
            say(STDOUT_FILENO, "%02d: ", step);
            write_all(STDOUT_FILENO, record, visible);
            write_all(STDOUT_FILENO, "\n", 1);
        }
    }
    if (missing > 0)
        say(STDOUT_FILENO, "Brakuje %d kroków, tort sie zapadnie!\n", missing);
    else
        say(STDOUT_FILENO, "Tort gotowy!\n");

    if (munmap(map, BOOK_SIZE) < 0)
        die("munmap");
}

int main(int argc, char **argv)
{
    if (argc < 3 || argc > MAX_TOWERS + 2) {
        say(STDERR_FILENO, "Uzycie: %s recipe tower1 [tower2 ... tower5]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int recipe_fd = open_recipe(argv[1]);
    int count = argc - 2;
    Tower towers[MAX_TOWERS] = {0};
    for (int i = 0; i < count; ++i) {
        towers[i].path = argv[i + 2];
        towers[i].fd = -1;
        check_tower(towers[i].path);
    }

    receive_steps(recipe_fd, towers, count);
    finish_recipe(recipe_fd);

    if (close(recipe_fd) < 0)
        die("close recipe");
    return EXIT_SUCCESS;
}

```

---

## Jak rozumieć pełny kod? Mapa funkcja → zadanie

| Funkcja | Co robi? | Dlaczego jej potrzebujesz? |
|---|---|---|
| `die(what)` | `perror` + zakończenie | Jedno miejsce do zgłaszania błędów systemowych |
| `write_all(fd,p,len)` | `write()` tak długo, aż wyśle całość | `write()` może wykonać krótszy zapis |
| `say(fd, format,...)` | Składa tekst przez `vsnprintf`, wysyła przez `write_all` | Komunikaty mają numery kroków i nazwy wież |
| `open_recipe(path)` | Tworzy 2048B albo sprawdza `lseek()` | Etap 1; nie kasuje istniejących danych |
| `save_step(...)` | Buduje 64B i nadpisuje rekord przez `pwrite` | Etap 2; dowolna kolejność kroków |
| `process_line(...)` | Waliduje prefiks `NN:` | Odrzuca błędne numeracje |
| `consume(...)` | Z fragmentów `read` składa linie po `\n` | Wiadomości nie muszą przychodzić w całości |
| `check_tower(path)` | `stat()` + `S_ISFIFO()` | Odrzuca nie-FIFO / nieistniejące ścieżki |
| `open_tower(path)` | Otwiera FIFO | Może blokować się, aż pojawi się nadawca |
| `receive_steps(...)` | Otwiera wieże, używa `poll`, czyta i zamyka FIFO | Etap 3, bez busy waiting |
| `finish_recipe(...)` | `mmap` → zamiana → wydruk → `munmap` | Etap 4 |
| `main(...)` | Sprawdza argumenty i wywołuje etapy | Kontrola kolejności i liczby wież |

### Które funkcje kopiować przy typowych zadaniach?

- **Kopiowanie zwykłych plików:** `write_all` + pętla `read` do EOF. Nie potrzebujesz `poll`, `mmap`, `Tower`.
- **Losowe nadpisywanie rekordów:** `save_step`, `pwrite`, `lseek`, `sizeof`, `memset`.
- **Jedno FIFO + wiadomości tekstowe:** `check_tower`, `open_tower`, `consume`, `process_line`; `poll` nie jest wymagany, ale może być użyteczny.
- **Wiele FIFO:** `Tower` + `receive_steps` + `poll`, przy czym **każde FIFO ma własny bufor**.
- **Zmiana tekstu pliku bez `read/write`:** `finish_recipe`: `mmap`, `memcmp`, `memcpy`, `msync`, `munmap`.

---

## Awaryjna diagnostyka · zobacz zanim spanikujesz

| Objaw | Najbardziej prawdopodobna przyczyna | Co sprawdzić / zrobić |
|---|---|---|
| `open` stoi i nic nie robi | FIFO otwarte do odczytu czeka na piszącego | Uruchom **najpierw** `granny.sh` w innym terminalu; to NORMALNE |
| `Nie ma babci na wieży ...` | Brak FIFO / zwykły plik pod daną nazwą | `ls -l tower1`: pierwszy znak uprawnień ma być **`p`** |
| `Ta ksiega to nie przepis!` | Istniejący plik ma inny rozmiar niż 2048 | `wc -c recipe.txt`; usuń plik albo użyj nowej nazwy, jeśli testujesz od nowa |
| `poll` zwraca `0` | Przez 3 s brak zdarzeń | Wypisz komunikat i czekaj ponownie. **Nie** `usleep` w busy loop |
| `poll` zwraca `-1`, `errno=EINTR` | Sygnał przerwał czekanie | `continue` i ponów `poll()` |
| `read` zwraca `0` | EOF: nadawca zamknął FIFO | Zamknij fd, ustaw `pollfd.fd=-1`, zmniejsz `active` |
| `read` zwraca `-1`, `errno=EINTR` | Sygnał przerwał odczyt | Ponów `read()` |
| `read` zwraca `-1`, `errno=EAGAIN` | Niekblokujące fd chwilowo bez danych | Nie traktuj jak EOF; czekaj przez `poll` (kod pełny używa blokujących FIFO) |
| `write` zapisał mniej | **Short write** | Wróć do `write_all`: `done += wynik_write` |
| `write` zwraca `-1`, `errno=EPIPE` | Brak odbiorcy łącza | Domyślnie możesz dostać też `SIGPIPE`; sprawdź nadawcę/odbiorcę |
| `mmap` zwraca błąd | Brak praw, zła długość, zły fd | Sprawdź `O_RDWR`, długość `2048`, `MAP_FAILED` |
| Po `MAP_PRIVATE` plik nie zmienił się | Prywatne mapowanie nie zapisuje do pliku | Użyj `MAP_SHARED` i dla synchronizacji `msync` |
| `grep -c '~' recipe.txt` nie jest 0 | Literówki nie zostały zamienione | Sprawdź `memcmp`, `memcpy`, `MAP_SHARED` i długość 11 |
| `printf("%s", buf)` wyświetla śmieci | Bufor z `read()` nie ma terminatora `\0` | `write_all(...,n)` lub zadbaj o miejsce na `\0` |
| `errno` niby `Success` po własnym błędzie | Użyto `perror()` do opisania logiki biznesowej | `fprintf(stderr, "komunikat\n")`; `perror` tylko po nieudanym syscallu |
| WSL nie chce FIFO | Próbujesz na ścieżce Windows `/mnt/c` lub FIFO nie istnieje | Spróbuj `mkdir -p ~/sop; cd ~/sop; mkfifo tower1` |
| `SIGPIPE` kończy nadawcę | Próba zapisu do pipe bez czytelników | Napraw synchronizację otwierania/zamykania; w razie potrzeby kontrolowana obsługa sygnału |
| `implicit declaration` / `pwrite` niezdefiniowane | Brak nagłówka albo feature macro | Dodaj `#define _POSIX_C_SOURCE 200809L` jako **pierwszą** linię, przed `#include` |

### Pięć błędów, które najłatwiej zrobić pod presją

1. **Nie dodawaj `n` z `read()` do `done` podczas pętli `write()`**. Dodawaj wyłącznie wynik ostatniego `write()`.
2. **Nie kończ odbioru na `POLLHUP` bez odczytu**. Najpierw odczytaj to, co jeszcze zostało, a dopiero `read()==0` kończy FIFO.
3. **Nie stosuj `strlen()` na buforze z surowego `read()`**. To bajty o podanej długości, nie gwarantowany łańcuch C.
4. **Nie używaj `==` do porównywania dwóch napisów**. `strcmp(a,b)==0` porównuje treść; `==` adresy.
5. **Nie używaj jednego bufora na wszystkie strumienie**. `tower1` i `tower2` mogą przysłać po połowie wiadomości naraz.

---

## Uruchomienie na laboratorium

### Wariant A: z uczelnianym `granny.sh`

`granny.sh` jest skryptem dołączonym do [oryginalnego zadania](https://sop.mini.pw.edu.pl/pl/sop1/lab/l2/example1/). Uruchom go **osobno** w terminalach, a nie w tej samej sesji bez zarządzania procesami.

```bash
# Terminal 1 (piszący)
./granny.sh tower1 8

# Terminal 2 (piszący)
./granny.sh tower2 8

# Terminal 3 (odbiorca)
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 sop-cake.c -o sop-cake
./sop-cake recipe.txt tower1 tower2

# Terminal 3: sprawdź wynik
wc -c recipe.txt      # ma być 2048
grep -c '~' recipe.txt # ma być 0 (gdy wszystkie literówki pasowały do wzorca)
cat recipe.txt
```

**Uwaga:** skrypt sam tworzy i usuwa FIFO. Jeśli dwa razy użyjesz tej samej nazwy, upewnij się, że poprzedni skrypt już się zakończył. Przepis **nie jest resetowany** przy kolejnym uruchomieniu: program celowo zachowuje poprzednie kroki. Do czystego testu wybierz inny plik albo usuń stary `recipe.txt`.

### Test ręczny bez `granny.sh`

```bash
# Wszystko w katalogu Linux / WSL, np. ~/sop
mkfifo tower1

# To uruchamia nadawcę w tle. printf wewnątrz nawiasów czeka,
# aż proces sop-cake otworzy tower1 do odczytu.
( printf '07:Top raw~berries!\n'; \
  printf '99:bledny numer\n'; \
  printf '01:Wiadomosc w '; sleep 1; printf 'dwoch czesciach\n' ) > tower1 &

./sop-cake recipe.txt tower1
wc -c recipe.txt
sed -n '1,9p' recipe.txt
rm -f tower1
```

**Sprawdź w głowie oczekiwane wyniki:** zapis kroku 07 i 01; błąd numeru 99; korekta `raw~berries → raspberries`; 2048 bajtów, 30 pustych kroków; `read` powinien poprawnie złożyć `Wiadomosc w dwoch czesciach`.

### Kontrola testów, które wykonano na autorskim rozwiązaniu

- ✅ Kompilacja `gcc -std=c11 -Wall -Wextra -Wpedantic -O2` bez ostrzeżeń.
- ✅ Dwa FIFO naraz, w tym odczyt jednego komunikatu rozłożonego na dwa fragmenty.
- ✅ Odrzucenie numeru `99` i komunikat błędu.
- ✅ Poprawienie `raw~berries` na `raspberries` i wynik pliku **2048 bajtów**.
- ✅ Odrzucenie niepoprawnego istniejącego pliku i ścieżki, która nie jest FIFO.

**Granice testu:** nie wykonano pełnego testu wszystkich losowych scenariuszy generowanych przez oficjalny `granny.sh`, awarii zasobów ani wszystkich możliwych wyścigów przy równoczesnym zamykaniu FIFO. To **materiał pomocniczy**, a nie gwarancja punktów od automatycznego sprawdzacza.

---

## Słowniczek funkcji na jedno spojrzenie

| Funkcja | Najprostszy opis | Wynik sukcesu | Błąd |
|---|---|---|---|
| `open(path, flags[,mode])` | Otwórz plik/FIFO | fd >= 0 | `-1`, `errno` |
| `stat(path,&st)` | Sprawdź typ i metadane | `0` | `-1` |
| `S_ISFIFO(st.st_mode)` | Czy to FIFO? | niezerowe | `0` to nie FIFO (nie syscall) |
| `lseek(fd,off,SEEK_END)` | Przesuń pozycję w pliku | nowy offset | `(off_t)-1` |
| `read(fd,buf,n)` | Czytaj maksymalnie n bajtów | >0 bajtów; 0 = EOF | `-1` |
| `write(fd,buf,n)` | Zapisz maksymalnie n bajtów | liczba zapisanych bajtów | `-1` |
| `pwrite(fd,buf,n,off)` | Zapisz w konkretnym miejscu | liczba zapisanych bajtów | `-1` |
| `poll(array,count,ms)` | Czekaj na zdarzenia z wielu fd | >0 zdarzenia; 0 timeout | `-1` |
| `mmap(...)` | Zmapuj plik do pamięci | wskaźnik | `MAP_FAILED` |
| `msync(p,len,MS_SYNC)` | Zsynchronizuj zmiany | `0` | `-1` |
| `munmap(p,len)` | Usuń mapowanie | `0` | `-1` |
| `close(fd)` | Zamknij deskryptor | `0` | `-1` |
| `memset(p,' ',63)` | Ustaw 63 bajty na spację | wskaźnik | nie raportuje błędu syscall |
| `memcmp(p,q,n)` | Porównaj n bajtów | 0 jeśli równe | nie ma sygnalizacji `errno` |
| `memcpy(p,q,n)` | Kopiuj n bajtów w pamięci | wskaźnik | nie ma sygnalizacji `errno` |

**Minimalny zestaw nagłówków z pełnego rozwiązania:**

```c
#define _POSIX_C_SOURCE 200809L
#include <errno.h>      // errno, EINTR, EEXIST
#include <fcntl.h>      // open, O_RDWR, O_CREAT, O_EXCL
#include <unistd.h>     // read, write, pwrite, lseek, close
#include <sys/stat.h>   // stat, S_ISFIFO
#include <poll.h>       // poll, pollfd, POLLIN, POLLHUP
#include <sys/mman.h>   // mmap, msync, munmap, MAP_SHARED
#include <string.h>     // memset, memcpy, memcmp
#include <stdio.h>      // perror, snprintf
#include <stdlib.h>     // exit, EXIT_FAILURE
```

**Ostatnia zasada:** najpierw uruchom **etap 1**, potem stopniowo zastępuj czytanie etapem 2, dodaj `poll` z etapu 3, a na końcu `mmap` z etapu 4. Jeśli pełny program zacznie się wywalać, wiesz wtedy, którą część odłączyć i sprawdzić. To oszczędza czas lepiej niż debugowanie wszystkiego naraz.
