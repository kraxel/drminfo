#include <stdbool.h>
#include <inttypes.h>

void logind_init(void);
void logind_fini(void);
bool have_logind(void);
int logind_open(const char *path, int flags, void *user_data);
void logind_close(int fd, void *user_data);

int device_open(const char *device);
