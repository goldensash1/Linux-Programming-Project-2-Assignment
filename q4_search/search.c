/*
 * search.c
 *
 * Multi-threaded keyword search across multiple text files.
 *
 * Each worker thread is responsible for one input file: it reads the file
 * with fopen/fread, counts (non-overlapping, case-sensitive) occurrences of
 * the given keyword, and appends "<filename>: <count>" to a shared output
 * file. Access to the shared output file is protected with a
 * pthread_mutex_t so writes from different threads never interleave.
 *
 * If there are more files than threads, threads are reused: each thread
 * pulls the next unprocessed file index from a shared, mutex-protected
 * work queue until no files remain (a simple thread-pool / work-stealing
 * scheme), which lets the program be tested meaningfully with 2 threads,
 * "average core count" threads, and "max threads" (one thread per file).
 *
 * Usage:
 *   ./search keyword output.txt file1.txt file2.txt ... <number_of_threads>
 *
 * Example:
 *   ./search error results.txt log1.txt log2.txt log3.txt log4.txt 4
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

typedef struct {
    char **filenames;
    int file_count;
    int next_index;          /* shared work-queue cursor */
    const char *keyword;
    FILE *out_fp;
    pthread_mutex_t queue_mutex;
    pthread_mutex_t write_mutex;
} shared_ctx_t;

/* Count non-overlapping occurrences of `keyword` in file `path`. */
static long count_occurrences_in_file(const char *path, const char *keyword) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "search: could not open '%s': %s\n", path, strerror(errno));
        return -1;
    }

    size_t keyword_len = strlen(keyword);
    size_t chunk_size = 1 << 16; /* 64 KB */
    /* overlap buffer so keyword occurrences spanning chunk boundaries are counted */
    size_t buf_cap = chunk_size + keyword_len;
    char *buf = malloc(buf_cap + 1);
    if (!buf) {
        fclose(fp);
        return -1;
    }

    long total = 0;
    size_t carry = 0; /* bytes carried over from previous chunk (overlap tail) */

    while (1) {
        size_t n = fread(buf + carry, 1, chunk_size, fp);
        if (n == 0) break;
        size_t data_len = carry + n;
        buf[data_len] = '\0';

        /* scan for keyword within buf[0..data_len) */
        size_t scan_limit = (n < chunk_size) ? data_len : (data_len - keyword_len + 1);
        for (size_t i = 0; i + keyword_len <= data_len && i < scan_limit; i++) {
            if (memcmp(buf + i, keyword, keyword_len) == 0) {
                total++;
                i += keyword_len - 1;
            }
        }

        if (n < chunk_size) break; /* EOF reached */

        /* keep the last (keyword_len - 1) bytes as overlap for next read */
        carry = (keyword_len > 1) ? keyword_len - 1 : 0;
        if (carry > 0) {
            memmove(buf, buf + data_len - carry, carry);
        }
    }

    free(buf);
    fclose(fp);
    return total;
}

static void *worker(void *arg) {
    shared_ctx_t *ctx = (shared_ctx_t *)arg;

    while (1) {
        pthread_mutex_lock(&ctx->queue_mutex);
        int idx = ctx->next_index;
        if (idx >= ctx->file_count) {
            pthread_mutex_unlock(&ctx->queue_mutex);
            break;
        }
        ctx->next_index++;
        pthread_mutex_unlock(&ctx->queue_mutex);

        const char *path = ctx->filenames[idx];
        long count = count_occurrences_in_file(path, ctx->keyword);

        pthread_mutex_lock(&ctx->write_mutex);
        if (count >= 0) {
            fprintf(ctx->out_fp, "%s: %ld\n", path, count);
        } else {
            fprintf(ctx->out_fp, "%s: ERROR\n", path);
        }
        fflush(ctx->out_fp);
        pthread_mutex_unlock(&ctx->write_mutex);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr,
            "Usage: %s keyword output.txt file1.txt [file2.txt ...] <number_of_threads>\n",
            argv[0]);
        return EXIT_FAILURE;
    }

    const char *keyword = argv[1];
    const char *output_path = argv[2];

    int num_threads = atoi(argv[argc - 1]);
    if (num_threads < 1) {
        fprintf(stderr, "Invalid number_of_threads: %s\n", argv[argc - 1]);
        return EXIT_FAILURE;
    }

    int file_count = argc - 4; /* argv[0]=prog, [1]=keyword, [2]=output, [last]=threads */
    if (file_count < 1) {
        fprintf(stderr, "No input files provided.\n");
        return EXIT_FAILURE;
    }
    char **filenames = &argv[3];

    if (num_threads > file_count) {
        num_threads = file_count; /* no point spawning more threads than files */
    }

    FILE *out_fp = fopen(output_path, "w");
    if (!out_fp) {
        perror("fopen output");
        return EXIT_FAILURE;
    }
    fprintf(out_fp, "Keyword search results for \"%s\" (threads=%d, files=%d)\n",
            keyword, num_threads, file_count);
    fprintf(out_fp, "--------------------------------------------------------\n");

    shared_ctx_t ctx;
    ctx.filenames = filenames;
    ctx.file_count = file_count;
    ctx.next_index = 0;
    ctx.keyword = keyword;
    ctx.out_fp = out_fp;
    pthread_mutex_init(&ctx.queue_mutex, NULL);
    pthread_mutex_init(&ctx.write_mutex, NULL);

    pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, worker, &ctx) != 0) {
            perror("pthread_create");
            return EXIT_FAILURE;
        }
    }
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    double elapsed = (t_end.tv_sec - t_start.tv_sec) +
                      (t_end.tv_nsec - t_start.tv_nsec) / 1e9;

    fprintf(out_fp, "--------------------------------------------------------\n");
    fprintf(out_fp, "Completed in %.6f seconds using %d thread(s)\n", elapsed, num_threads);
    fclose(out_fp);

    printf("Search complete. keyword=\"%s\" files=%d threads=%d elapsed=%.6fs\n",
           keyword, file_count, num_threads, elapsed);
    printf("Results written to '%s'\n", output_path);

    pthread_mutex_destroy(&ctx.queue_mutex);
    pthread_mutex_destroy(&ctx.write_mutex);
    free(threads);

    return EXIT_SUCCESS;
}
