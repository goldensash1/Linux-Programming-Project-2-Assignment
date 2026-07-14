/*
 * copy_stdio.c - Version 2: large-file copy using standard I/O (buffered).
 *
 * Uses fopen()/fread()/fwrite()/fclose() from the C standard library,
 * which perform their own internal buffering on top of the underlying
 * read()/write() system calls.
 *
 * Usage: ./copy_stdio <source> <destination> [buffer_size_bytes]
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DEFAULT_BUF_SIZE 65536  /* 64 KB */

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source> <destination> [buffer_size_bytes]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *src_path = argv[1];
    const char *dst_path = argv[2];
    size_t buf_size = (argc > 3) ? (size_t)atol(argv[3]) : DEFAULT_BUF_SIZE;

    FILE *src = fopen(src_path, "rb");
    if (!src) {
        perror("fopen source");
        return EXIT_FAILURE;
    }

    FILE *dst = fopen(dst_path, "wb");
    if (!dst) {
        perror("fopen destination");
        fclose(src);
        return EXIT_FAILURE;
    }

    char *buffer = malloc(buf_size);
    if (!buffer) {
        fprintf(stderr, "malloc failed for buffer size %zu\n", buf_size);
        fclose(src);
        fclose(dst);
        return EXIT_FAILURE;
    }

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    size_t n_read;
    long total_bytes = 0;
    long n_fread_calls = 0, n_fwrite_calls = 0;

    while ((n_read = fread(buffer, 1, buf_size, src)) > 0) {
        n_fread_calls++;
        size_t n_written = fwrite(buffer, 1, n_read, dst);
        n_fwrite_calls++;
        if (n_written != n_read) {
            fprintf(stderr, "fwrite short write\n");
            break;
        }
        total_bytes += (long)n_read;
    }

    fflush(dst);
    clock_gettime(CLOCK_MONOTONIC, &t_end);

    fclose(src);
    fclose(dst);
    free(buffer);

    double elapsed = (t_end.tv_sec - t_start.tv_sec) +
                      (t_end.tv_nsec - t_start.tv_nsec) / 1e9;

    printf("[copy_stdio] buffer size    : %zu bytes\n", buf_size);
    printf("[copy_stdio] bytes copied   : %ld\n", total_bytes);
    printf("[copy_stdio] fread() calls  : %ld\n", n_fread_calls);
    printf("[copy_stdio] fwrite() calls : %ld\n", n_fwrite_calls);
    printf("[copy_stdio] elapsed time   : %.6f seconds\n", elapsed);

    return EXIT_SUCCESS;
}
