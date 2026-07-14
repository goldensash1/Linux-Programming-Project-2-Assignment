/*
 * pipeline_ipc.c
 *
 * Demonstrates process creation (fork), program replacement (execvp),
 * inter-process communication (pipe) and file I/O.
 *
 * Behaviour: builds the equivalent of the shell pipeline
 *
 *      ps aux | grep <keyword>
 *
 * using two child processes connected by a pipe. The parent process does
 * NOT print to the terminal directly; instead it redirects the final
 * output of the pipeline into a file (pipeline_output.txt), then reopens
 * that file, reads it back and prints the first N lines to the screen.
 *
 * Usage:
 *      ./pipeline_ipc [keyword] [output_file]
 *      ./pipeline_ipc root pipeline_output.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

#define LINES_TO_SHOW 10

int main(int argc, char *argv[]) {
    const char *keyword = (argc > 1) ? argv[1] : "root";
    const char *outfile = (argc > 2) ? argv[2] : "pipeline_output.txt";

    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    /* ---------- Child 1: ps aux -> writes into pipe ---------- */
    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("fork (child1)");
        exit(EXIT_FAILURE);
    }

    if (pid1 == 0) {
        /* Child 1: stdout -> pipe write end */
        close(fd[0]);                 /* not reading */
        dup2(fd[1], STDOUT_FILENO);   /* stdout goes into the pipe */
        close(fd[1]);

        char *args[] = {"ps", "aux", NULL};
        execvp(args[0], args);
        perror("execvp ps");          /* only reached on failure */
        _exit(EXIT_FAILURE);
    }

    /* ---------- Child 2: grep <keyword> -> writes into outfile ------- */
    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("fork (child2)");
        exit(EXIT_FAILURE);
    }

    if (pid2 == 0) {
        /* Child 2: stdin <- pipe read end, stdout -> output file */
        close(fd[1]);                 /* not writing to pipe */
        dup2(fd[0], STDIN_FILENO);    /* stdin comes from the pipe */
        close(fd[0]);

        int out_fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd < 0) {
            perror("open outfile");
            _exit(EXIT_FAILURE);
        }
        dup2(out_fd, STDOUT_FILENO);
        close(out_fd);

        char *args[] = {"grep", (char *)keyword, NULL};
        execvp(args[0], args);
        perror("execvp grep");
        _exit(EXIT_FAILURE);
    }

    /* ---------- Parent: close both ends, wait, then read file back ---- */
    close(fd[0]);
    close(fd[1]);

    int status1, status2;
    waitpid(pid1, &status1, 0);
    waitpid(pid2, &status2, 0);

    printf("[parent] child1 (ps aux)  pid=%d exited with status %d\n", pid1, WEXITSTATUS(status1));
    printf("[parent] child2 (grep %s) pid=%d exited with status %d\n", keyword, pid2, WEXITSTATUS(status2));
    printf("[parent] pipeline output captured in '%s'\n\n", outfile);

    /* Parent re-opens the output file and displays the first N lines */
    FILE *fp = fopen(outfile, "r");
    if (!fp) {
        perror("fopen outfile for reading");
        exit(EXIT_FAILURE);
    }

    printf("---- First %d line(s) of '%s' ----\n", LINES_TO_SHOW, outfile);
    char line[1024];
    int count = 0;
    while (count < LINES_TO_SHOW && fgets(line, sizeof(line), fp)) {
        fputs(line, stdout);
        count++;
    }
    if (count == 0) {
        printf("(no matching lines found for keyword '%s')\n", keyword);
    }
    fclose(fp);

    return 0;
}
