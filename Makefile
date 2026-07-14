CC = gcc
CFLAGS = -O2 -Wall
LDLIBS_MATH = -lm
LDLIBS_PTHREAD = -lpthread

.PHONY: all clean q1 q2 q3 q4

all: q1 q2 q3 q4

q1: q1_pipeline/pipeline_ipc

q1_pipeline/pipeline_ipc: q1_pipeline/pipeline_ipc.c
	$(CC) $(CFLAGS) -o $@ $<

q2: q2_filecopy/copy_syscall q2_filecopy/copy_stdio

q2_filecopy/copy_syscall: q2_filecopy/copy_syscall.c
	$(CC) $(CFLAGS) -o $@ $<

q2_filecopy/copy_stdio: q2_filecopy/copy_stdio.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS_MATH)

q3: q3_primes/primes_mt

q3_primes/primes_mt: q3_primes/primes_mt.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS_PTHREAD) $(LDLIBS_MATH)

q4: q4_search/search

q4_search/search: q4_search/search.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS_PTHREAD)

clean:
	rm -f q1_pipeline/pipeline_ipc \
	      q2_filecopy/copy_syscall q2_filecopy/copy_stdio \
	      q3_primes/primes_mt \
	      q4_search/search q4_search/gen_testfiles
