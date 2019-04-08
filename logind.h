#include <stdbool.h>
#include <inttypes.h>

#include <libudev.h>
#include <libinput.h>

void logind_init(void);
void logind_fini(void);
int logind_open(const char *path);
