#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <gbm.h>

/* ------------------------------------------------------------------ */

int drm_init_dev(const char *devname)
{
    uint64_t prime, dumb;
    int fd, rc;

    /* open device */
    fd = open(devname, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", devname, strerror(errno));
        exit(1);
    }

    rc = drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &dumb);
    if (rc < 0) {
        fprintf(stderr, "drmGetCap(DRM_CAP_DUMB_BUFFER): %s\n", strerror(errno));
        exit(1);
    }
    rc = drmGetCap(fd, DRM_CAP_PRIME, &prime);
    if (rc < 0) {
        fprintf(stderr, "drmGetCap(DRM_CAP_PRIME): %s\n", strerror(errno));
        exit(1);
    }

    fprintf(stderr, "%s:\n", devname);
    fprintf(stderr, "    dumb buffers: %s\n",
            dumb ? "yes" : "no");
    fprintf(stderr, "    prime import: %s\n",
            prime & DRM_PRIME_CAP_IMPORT ? "yes" : "no");
    fprintf(stderr, "    prime export: %s\n",
            prime & DRM_PRIME_CAP_EXPORT ? "yes" : "no");

    return fd;
}

/* ------------------------------------------------------------------ */

static void usage(FILE *fp)
{
    fprintf(fp,
            "\n"
            "usage: prime [ options ]\n"
            "\n"
            "options:\n"
            "  -h         print this\n"
            "\n");
}

int main(int argc, char **argv)
{
    char devname[64];
    int card, c, i;

    for (;;) {
        c = getopt(argc, argv, "h");
        if (c == -1)
            break;
        switch (c) {
        case 'h':
            usage(stdout);
            exit(0);
        default:
            usage(stderr);
            exit(1);
        }
    }

    for (i = 0;; i++) {
        snprintf(devname, sizeof(devname), DRM_DEV_NAME, DRM_DIR_NAME, i);
        if (access(devname, R_OK | W_OK) != 0)
            break;
        card = drm_init_dev(devname);
        close(card);
    }

    return 0;
}
