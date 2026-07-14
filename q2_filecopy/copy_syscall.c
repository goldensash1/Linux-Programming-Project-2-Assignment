/*
 * copy_syscall.c - Version 1: large-file copy using low-level system calls.
 *
 * Uses open()/read()/write()/close() directly (no libc buffering).
 * Reports wall-clock execution time using clock_gettime(CLOCK_MONOTONIC).
 *
 * Usage: ./copy_syscall <source> <destination> [buffer_size_bytes]
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>

#define DEFAULT_BUF_SIZE 65536  /* 64 KB */

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source> <destination> [buffer_size_bytes]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *src_path = argv[1];
    const char *dst_path = argv[2];
    size_t buf_size = (argc > 3) ? (size_t)atol(argv[3]) : DEFAULT_BUF_SIZE;

    int src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) {
        perror("open source");
        return EXIT_FAILURE;
    }

    int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
        perror("open destination");
        close(src_fd);
        return EXIT_FAILURE;
    }

    char *buffer = malloc(buf_size);
    if (!buffer) {
        fprintf(stderr, "malloc failed for buffer size %zu\n", buf_size);
        close(src_fd);
        close(dst_fd);
        return EXIT_FAILURE;
    }

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    ssize_t n_read;
    long total_bytes = 0;
    long n_read_calls = 0, n_write_calls = 0;

    while ((n_read = read(src_fd, buffer, buf_size)) > 0) {
        n_read_calls++;
        ssize_t written = 0;
        while (written < n_read) {
            ssize_t n_written = write(dst_fd, buffer + written, n_read - written);
            if (n_written < 0) {
                perror("write");
                free(buffer);
                close(src_fd);
                close(dst_fd);
                return EXIT_FAILURE;
            }
            written += n_written;
            n_write_calls++;
        }
        total_bytes += n_read;
    }

    if (n_read < 0) {
        perror("read");
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);

    close(src_fd);
    close(dst_fd);
    free(buffer);

    double elapsed = (t_end.tv_sec - t_start.tv_sec) +
                      (t_end.tv_nsec - t_start.tv_nsec) / 1e9;

    printf("[copy_syscall] buffer size   : %zu bytes\n", buf_size);
    printf("[copy_syscall] bytes copied  : %ld\n", total_bytes);
    printf("[copy_syscall] read() calls  : %ld\n", n_read_calls);
    printf("[copy_syscall] write() calls : %ld\n", n_write_calls);
    printf("[copy_syscall] elapsed time  : %.6f seconds\n", elapsed);

    return EXIT_SUCCESS;
}
