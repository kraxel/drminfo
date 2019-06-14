#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "drm-lease.h"

int drm_lease(const char *output)
{
    if (getenv("DISPLAY") != NULL) {
#ifdef HAVE_XRANDR
        return drm_lease_xserver(output);
#else
        fprintf(stderr, "drm-lease: compiled without xrandr support.\n");
        exit(1);
#endif
    }

    fprintf(stderr, "drm-lease: don't know where to lease from.\n");
    exit(1);
}
