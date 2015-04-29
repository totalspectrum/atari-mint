#define TICKS_PER_TOCK		200
#define TOCKS_PER_SECOND	1

#define SAMPS_PER_MIN	12
#define SAMPS_PER_5MIN	SAMPS_PER_MIN * 5
#define SAMPS_PER_15MIN	SAMPS_PER_MIN * 15

#define LOAD_SCALE 2048

extern unsigned long uptime;
extern unsigned long avenrun[3];
