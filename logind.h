#include <stdbool.h>
#include <inttypes.h>

void logind_init(void);
void logind_fini(void);
int logind_open(const char *path);

int device_open(const char *device);
