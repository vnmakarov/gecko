void printf (const char *fmt, ...);
void abort (void);
#define SieveSize 819000
#define N_ITER 1
#define SIEVE(nn)                                                      \
  int sieve##nn (int n) {                                              \
    long i, k, count, iter, prime;                                     \
    char flags[SieveSize];                                             \
                                                                       \
    for (iter = 0; iter < n; iter++) {                                 \
      count = 0;                                                       \
      for (i = 0; i < SieveSize; i++) flags[i] = 1;                    \
      for (i = 2; i < SieveSize; i++)                                  \
        if (flags[i]) {                                                \
          prime = i + 1;                                               \
          for (k = i + prime; k < SieveSize; k += prime) flags[k] = 0; \
          count++;                                                     \
        }                                                              \
    }                                                                  \
    return count;                                                      \
  }

#include "test_sieve.h"

int main (void) {
  int n = sieve0 (N_ITER);

  printf ("%d iterations of sieve for %d: result = %d\n", N_ITER, SieveSize, n);
  return 0;
}
