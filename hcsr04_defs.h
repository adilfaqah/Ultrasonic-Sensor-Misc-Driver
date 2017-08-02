#ifndef hcsr04_defs_h
#define hcsr04_defs_h

#include <linux/ioctl.h>

#define MAGIC_NUMBER 0x87
#define SETPINS _IOW(MAGIC_NUMBER, 1, int)
#define SETMODE _IOW(MAGIC_NUMBER, 2, int)

#define ONESHOT 0
#define PERIODIC 1
#define N 2 //Number of misc devices

//Preprocessor directives to change the color of console output text.
#define KNRM  "\x1B[0m"
#define KCYAN "\e[0;36m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define RESET "\033[0m"
#define BOLD "\e[1m"

#endif