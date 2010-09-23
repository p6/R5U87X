#ifndef _CONFIG_H_
#define _CONFIG_H_

#define VERSION "0.2"

// By default, defined in Makefile
//#define UCODE_PATH "/usr/lib/r5u87x/ucode/r5u87x-%vid%-%pid%.fw"

// Provide support for --reload argument in application
// so that we can reload the uvcvideo kernel module
#define ENABLE_RELOAD

#endif
