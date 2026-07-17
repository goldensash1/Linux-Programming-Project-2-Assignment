# Project 2 Linux Programming: System Calls, strace, and Multithreading

This repository contains four C programs demonstrating process creation and
IPC, low-level vs. buffered file I/O, and POSIX multithreading with
synchronization, together with the `strace` traces and performance data
collected while testing them, and a detailed analytical report.

All programs are written in standard C (C11) using POSIX APIs
(`unistd.h`, `pthread.h`, `sys/wait.h`, etc.) and were built and tested on
**Ubuntu 22.04 (Linux, aarch64/x86_64)** using `gcc` and `strace`. They do
**not** run natively on macOS/Windows because they rely on Linux-specific
system-call semantics and the `strace` tool. A `Dockerfile` is included so
the exact same Ubuntu environment used for testing can be reproduced on any
machine with Docker installed.

## Repository layout

```
.
├── Dockerfile                 # Ubuntu 22.04 + build-essential + strace
├── Makefile                   # builds all four programs
├── q1_pipeline/
│   └── pipeline_ipc.c         # Q1: fork + execvp + pipe
├── q2_filecopy/
│   ├── copy_syscall.c         # Q2 v1: read()/write()
│   └── copy_stdio.c           # Q2 v2: fread()/fwrite()
├── q3_primes/
│   └── primes_mt.c            # Q3: 16-thread prime counter
├── q4_search/
│   ├── search.c                # Q4: multithreaded keyword search
│   ├── gen_testfiles.c         # helper: generates synthetic log files for testing
│   └── results_*threads.txt    # sample output captured during testing
├── strace_logs/                # captured strace output used in the report
└── docs/
    └── Project2_Report.docx    # the detailed analytical report
```

## Building

With Docker (recommended, guarantees a Linux environment with `strace`):

```bash
docker build -t proj2-linux -f Dockerfile .
docker run -it --rm --cap-add=SYS_PTRACE -v "$(pwd)":/work proj2-linux bash
cd /work
make
```

Directly on a Linux machine that already has `gcc`, `make`, and `strace`:

```bash
make          # builds all four binaries
# or individually:
make q1
make q2
make q3
make q4
```

## Question 1 Process creation, `execvp`, and `pipe` IPC

**Program:** [`q1_pipeline/pipeline_ipc.c`](q1_pipeline/pipeline_ipc.c)

Recreates the shell pipeline `ps aux | grep <keyword>` using two child
processes connected by a `pipe()`:

* **Child 1**  `dup2`s its stdout onto the pipe's write end and
  `execvp`s `ps aux`.
* **Child 2**  `dup2`s its stdin onto the pipe's read end, `dup2`s its
  stdout onto an output file opened with `open(O_WRONLY|O_CREAT|O_TRUNC)`,
  and `execvp`s `grep <keyword>`.
* **Parent**  closes both pipe ends (critical so child 2 sees EOF once
  child 1 finishes), `waitpid()`s on both children, then reopens the
  output file with `fopen()` and prints the first 10 lines to the
  terminal.

Run:

```bash
./q1_pipeline/pipeline_ipc root pipeline_output.txt
```

Trace process creation, pipe, and file operations:

```bash
strace -f -tt -e trace=process,pipe,pipe2,fork,vfork,clone,execve,dup2,dup3,close,read,write,open,openat \
    -o strace_logs/q1_strace_filtered.log ./q1_pipeline/pipeline_ipc root pipeline_output.txt
```

The full and filtered traces captured during development are in
[`strace_logs/q1_strace_full.log`](strace_logs/q1_strace_full.log) and
[`strace_logs/q1_strace_filtered.log`](strace_logs/q1_strace_filtered.log).
A syscall-by-syscall walkthrough is in the report.

## Question 2 System calls vs. standard I/O for large-file copy

**Programs:**
[`q2_filecopy/copy_syscall.c`](q2_filecopy/copy_syscall.c) (low-level
`read`/`write`) and
[`q2_filecopy/copy_stdio.c`](q2_filecopy/copy_stdio.c) (buffered
`fread`/`fwrite`).

Both take the same arguments and print bytes copied, number of I/O calls
made, and elapsed wall-clock time (`clock_gettime(CLOCK_MONOTONIC)`):

```bash
./q2_filecopy/copy_syscall <source> <destination> [buffer_size_bytes]
./q2_filecopy/copy_stdio   <source> <destination> [buffer_size_bytes]
```

Generate a 150 MB test file and run both:

```bash
dd if=/dev/urandom of=testfile_150MB.bin bs=1M count=150
./q2_filecopy/copy_syscall testfile_150MB.bin out_syscall.bin
./q2_filecopy/copy_stdio   testfile_150MB.bin out_stdio.bin
cmp testfile_150MB.bin out_syscall.bin   # verify correctness
cmp testfile_150MB.bin out_stdio.bin
```

Count system calls with `strace -c`:

```bash
strace -c ./q2_filecopy/copy_syscall testfile_150MB.bin out_syscall.bin
strace -c ./q2_filecopy/copy_stdio   testfile_150MB.bin out_stdio.bin
```

Captured strace summaries: [`strace_logs/q2_syscall_straceC.log`](strace_logs/q2_syscall_straceC.log),
[`strace_logs/q2_stdio_straceC.log`](strace_logs/q2_stdio_straceC.log).
Full numbers, timings across buffer sizes (4 KB / 64 KB / 1 MB), and the
explanation of *why* the two versions differ are in the report.

## Question 3 16-thread prime counter with `pthread_mutex_t`

**Program:** [`q3_primes/primes_mt.c`](q3_primes/primes_mt.c)

Splits the range `[1, 200000]` into 16 equal contiguous chunks (12,500
numbers each), spawns one `pthread_t` per chunk, and has each thread count
primes locally (trial division up to `sqrt(n)`) before acquiring a shared
`pthread_mutex_t` to add its local count into the global total 
minimizing lock contention since the mutex is only held for one addition
per thread, not per number.

```bash
./q3_primes/primes_mt
```

Expected final line:

```
The synchronized total number of prime numbers between 1 and 200000 is 17984
```

(17984 is the correct, independently-verifiable count of primes below
200,000.)

## Question 4 Multithreaded keyword search across files

**Program:** [`q4_search/search.c`](q4_search/search.c)

```bash
./q4_search/search keyword output.txt file1.txt file2.txt ... <number_of_threads>
```

* Each worker thread pulls the next unprocessed filename from a shared,
  mutex-protected work index (a simple work queue), so the program behaves
  correctly whether there are more threads than files, fewer, or exactly
  one thread per file.
* Each thread reads its file with `fopen`/`fread` in 64 KB chunks (with a
  small overlap so occurrences spanning a chunk boundary aren't missed),
  counts non-overlapping occurrences of the keyword, and appends
  `"<file>: <count>"` to the shared output file. Writes to the shared
  output file are serialized with a second `pthread_mutex_t` so lines from
  different threads never interleave or get lost.
* If more threads are requested than there are files, the thread count is
  clamped down to the file count (there is no benefit  and no way  to
  usefully run more workers than there is work).

`gen_testfiles.c` is a small helper (not part of the graded deliverable)
that generates reproducible synthetic log files for testing:

```bash
gcc -O2 -o q4_search/gen_testfiles q4_search/gen_testfiles.c
mkdir -p q4_search/testfiles
for i in $(seq 1 20); do
    ./q4_search/gen_testfiles q4_search/testfiles/log$i.txt $i 60000
done
```

Tested configurations (20 files, ~5 MB / 60,000 lines each, on a 10-core
container so the three required configurations are genuinely distinct):

| Threads | Meaning | Elapsed (median of 3) |
|---|---|---|
| 2  | fixed low-parallelism baseline | ~0.130 s |
| 10 | average number of CPU cores (`nproc` = 10) | ~0.038 s |
| 20 | maximum threads (one thread per file) | ~0.035 s |

Speedup is near-linear up to the physical core count (~3.5× from 2 → 10
threads) and flat beyond it (20 threads ≈ 10 threads), since threads
beyond the core count cannot execute in parallel. Per-file counts are
byte-for-byte identical across all three configurations and match an
independent `grep -o keyword file | wc -l` check.

Sample outputs: [`q4_search/results_2threads.txt`](q4_search/results_2threads.txt),
[`q4_search/results_10threads.txt`](q4_search/results_10threads.txt),
[`q4_search/results_20threads.txt`](q4_search/results_20threads.txt).

## Report

The full analytical report  syscall-by-syscall trace analysis for Q1,
strace-based performance comparison for Q2, and synchronization/
performance discussion for Q3 and Q4  is in
[`docs/Project2_Report.docx`](docs/Project2_Report.docx).

## Environment note

Development and testing were done inside an Ubuntu 22.04 Docker container
(see `Dockerfile`) because `strace` and Linux process-tracing semantics
are not available on macOS. All source code is portable standard
POSIX/C and will build identically on any Linux distribution with
`gcc`, `make`, and (for tracing) `strace` installed.
