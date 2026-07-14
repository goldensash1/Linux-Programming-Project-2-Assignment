/*
 * primes_mt.c
 *
 * Counts the number of prime numbers in the range [1, 200000] using
 * NUM_THREADS POSIX threads. The range is divided into equal contiguous
 * segments (one per thread). Each thread counts primes in its own segment
 * and adds its local result into a shared global counter, protected by a
 * pthread_mutex_t.
 *
 * Expected output:
 *   The synchronized total number of prime numbers between 1 and 200000 is <total>
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>

#define UPPER_LIMIT 200000
#define NUM_THREADS 16

static long shared_prime_count = 0;
static pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    long start;   /* inclusive */
    long end;     /* inclusive */
    int thread_id;
} range_t;

static int is_prime(long n) {
    if (n < 2) return 0;
    if (n < 4) return 1;              /* 2 and 3 */
    if (n % 2 == 0) return 0;
    long limit = (long)sqrt((double)n);
    for (long i = 3; i <= limit; i += 2) {
        if (n % i == 0) return 0;
    }
    return 1;
}

static void *count_primes_worker(void *arg) {
    range_t *range = (range_t *)arg;
    long local_count = 0;

    for (long n = range->start; n <= range->end; n++) {
        if (is_prime(n)) local_count++;
    }

    pthread_mutex_lock(&count_mutex);
    shared_prime_count += local_count;
    pthread_mutex_unlock(&count_mutex);

    printf("Thread %2d processed range [%6ld, %6ld] -> found %ld primes\n",
           range->thread_id, range->start, range->end, local_count);

    free(range);
    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];
    long chunk = UPPER_LIMIT / NUM_THREADS;

    for (int i = 0; i < NUM_THREADS; i++) {
        range_t *range = malloc(sizeof(range_t));
        range->start = i * chunk + 1;
        range->end = (i == NUM_THREADS - 1) ? UPPER_LIMIT : (i + 1) * chunk;
        range->thread_id = i;

        if (pthread_create(&threads[i], NULL, count_primes_worker, range) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\nThe synchronized total number of prime numbers between 1 and %d is %ld\n",
           UPPER_LIMIT, shared_prime_count);

    pthread_mutex_destroy(&count_mutex);
    return 0;
}
