extern int debug_level;				/* in debug.c */
extern int debug_logging;			/* in debug.c */

#define FORCE_LEVEL 0
#define ALERT_LEVEL 1
#define DEBUG_LEVEL 2
#define TRACE_LEVEL 3
#define LOW_LEVEL 4

#ifndef DEBUG_INFO

# define TRACELOW(x)
# define TRACE(x)
# define DEBUG(x)

#else                         

# define TRACELOW(s) \
  do { if (debug_logging || (debug_level >= LOW_LEVEL)) \
         Tracelow s ; } while(0)

# define TRACE(s) \
  do { if (debug_logging || (debug_level >= TRACE_LEVEL)) \
         Trace s ; } while(0)

# define DEBUG(s) \
  do { if (debug_logging || (debug_level >= DEBUG_LEVEL)) \
         Debug s ; } while(0)

#endif /* DEBUG_INFO */
