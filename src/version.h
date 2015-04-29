#define MAJ_VERSION	1
#define MIN_VERSION	12

#if 0
#define BETA "BETA"
#endif

#ifdef BETA
#define VERS_STRING	"%d.%02d" BETA
#else
#define VERS_STRING	"%d.%02d"
#endif
