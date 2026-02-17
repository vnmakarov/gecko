/* This file is a part of Gecko Parser (GLR Parser) project.
   Copyright (C) 2026 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#ifndef __TICKER__

#define __TICKER__

#include <stdio.h>
#include <time.h>

/* The following structure describes a ticker. */
struct ticker {
  /* The following member value is time of the ticker creation with taking into account time when
     the ticker is off.  Active time of the ticker is current time minus the value. */
  double modified_creation_time;
  /* The following member value is time (when the ticker was off.
     Negative value means that now the ticker is on. */
  double incremented_off_time;
};

/* The ticker is represented by the following type. */
typedef struct ticker ticker_t;

#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 199309L

static inline double clock_get () {
  struct timespec tv;
  clock_gettime (CLOCK_MONOTONIC, &tv);
  return (double) tv.tv_sec + (double) tv.tv_nsec / 1.0e9;
}

#else
/* We don't use times or getrusage here because they have the same
   accuracy as clock on major machines Linux, Solaris, AIX.  The
   single difference is IRIX on which getrusage is more accurate. */

/* The following macro is necessary for non standard include files of
   SUNOS 4..., linux */

#ifndef CLOCKS_PER_SECOND
#ifdef CLOCKS_PER_SEC
#define CLOCKS_PER_SECOND CLOCKS_PER_SEC
#elif __linux__
#define CLOCKS_PER_SECOND 100
#elif sun
#define CLOCKS_PER_SECOND 1000000
#elif CLK_TCK
#define CLOCKS_PER_SECOND CLK_TCK
#else
#error define macro CLOCKS_PER_SECOND
#endif
#endif /* CLOCKS_PER_SECOND */

static inline double clock_get () { return (double) clock () / (double) CLOCKS_PER_SECOND; }
#endif /* _POSIX_C_SOURCE >= 199309L */

/* The following function creates ticker and makes it active. */
static inline ticker_t create_ticker (void) {
  ticker_t ticker;
  ticker.modified_creation_time = clock_get ();
  ticker.incremented_off_time = -1.0;
  return ticker;
}

/* The following function switches off given ticker. */
static inline void ticker_off (ticker_t *ticker) {
  if (ticker->incremented_off_time < 0) ticker->incremented_off_time = clock_get ();
}

/* The following function switches on given ticker. */
static inline void ticker_on (ticker_t *ticker) {
  if (ticker->incremented_off_time >= 0.0) {
    ticker->modified_creation_time += clock_get () - ticker->incremented_off_time;
    ticker->incremented_off_time = -1.0;
  }
}

/* The following function returns current time since the moment when given ticker was created.  The
   result is measured in seconds as float number. */
static inline double active_time (ticker_t ticker) {
  if (ticker.incremented_off_time >= 0.0)
    return ticker.incremented_off_time - ticker.modified_creation_time;
  else
    return clock_get () - ticker.modified_creation_time;
}

/* The following function returns string representation of active time of given ticker.  The result
   is string representation of seconds with accuracy of 1/100 second.  Only result of the last call
   of the function exists.  Therefore the following code is not correct

      printf ("parser time: %s\ngeneration time: %s\n",
              active_time_string (parser_ticker),
              active_time_string (generation_ticker));

   Correct code has to be the following

      printf ("parser time: %s\n", active_time_string (parser_ticker));
      printf ("generation time: %s\n",
              active_time_string (generation_ticker));

*/

static inline const char *active_time_string (ticker_t ticker) {
  static char str[40];
  sprintf (str, "%.6f", active_time (ticker));
  return str;
}

#endif /* __TICKER__ */
