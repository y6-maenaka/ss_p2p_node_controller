#ifndef BE8912BF_20F6_4F98_A0AF_1A4E53A37F92
#define BE8912BF_20F6_4F98_A0AF_1A4E53A37F92


#if !defined(LOGGER_TIME_FORMAT) || !defined(LOGGER_OUTFILE_TIME_FORMAT)
#define LOGGER_TIME_FORMAT "%H:%M:%S"
#define LOGGER_OUTFILE_TIME_FORMAT "%Y_%m_%d"
#endif

#if !defined(LOGGER_MINIMUM_PREFIX_FORMAT) || !defined(LOGGER_PREFIX_FORMAT)
#define LOGGER_MINIMUM_PREFIX_FORMAT "[{TIME}][{SERITY}] "
#define LOGGER_PREFIX_FORMAT "[{TIME}][{SERITY}] {CUSTOM} "
#endif

#ifndef LOGGER_OUTFILE_FORMAT
#define LOGGER_OUTFILE_FORMAT "{EXE}_{DATE}.log"
#endif


#endif
