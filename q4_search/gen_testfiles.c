/* Helper (not part of the graded deliverable): generates synthetic log
 * files used to exercise search.c with a known, reproducible keyword
 * frequency. Not required by the assignment; kept for test reproducibility. */
#include <stdio.h>
#include <stdlib.h>

static const char *words[] = {
    "error", "warning", "info", "debug", "system", "network",
    "fail", "success", "timeout", "retry", "error", "connection"
};
#define NUM_WORDS (sizeof(words) / sizeof(words[0]))

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <outfile> <seed> <lines>\n", argv[0]);
        return 1;
    }
    FILE *f = fopen(argv[1], "w");
    srand((unsigned)atoi(argv[2]));
    long lines = atol(argv[3]);
    for (long l = 0; l < lines; l++) {
        for (int w = 0; w < 12; w++) {
            fputs(words[rand() % NUM_WORDS], f);
            fputc(w == 11 ? '\n' : ' ', f);
        }
    }
    fclose(f);
    return 0;
}
