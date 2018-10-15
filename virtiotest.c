#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>

#include <sys/ioctl.h>
#include "virtgpu_drm.h"

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cairo.h>
#include <pixman.h>

#include "drmtools.h"

/* ------------------------------------------------------------------ */

static struct {
    uint64_t cap;
    const char *name;
} virtio_caps[] = {
    { .cap = VIRTGPU_PARAM_3D_FEATURES,       .name = "virgl-3d"    },
#ifdef VIRTGPU_PARAM_CAPSET_QUERY_FIX
    { .cap = VIRTGPU_PARAM_CAPSET_QUERY_FIX,  .name = "capset-fix"  },
#endif
};

static uint64_t virtio_get_cap(uint64_t cap)
{
    struct drm_virtgpu_getparam param = {
        .param = cap,
        .value = 0,
    };
    int ret;

    ret = ioctl(fd, DRM_VIRTGPU_GETPARAM, &param);
    if (ret != 0) {
        fprintf(stderr, "ioctl DRM_VIRTGPU_GETPARAM(%" PRId64 "): %s\n",
                cap, strerror(errno));
        exit(1);
    }
    return param.value;
}

static void virtio_print_caps(void)
{
    uint64_t value;
    int i;

    printf("virtio capabilities\n");
    for (i = 0; i < sizeof(virtio_caps)/sizeof(virtio_caps[0]); i++) {
        value = virtio_get_cap(virtio_caps[i].cap);
        printf("    %s: %" PRId64 "\n", virtio_caps[i].name, value);
    }
}

/* ------------------------------------------------------------------ */

static void usage(FILE *fp)
{
    fprintf(fp,
            "\n"
            "usage: virtiotest [ options ]\n"
            "\n"
            "options:\n"
            "  -h         print this\n"
            "  -c <nr>    pick card\n"
            "  -i         print device info\n"
            "\n");
}

int main(int argc, char **argv)
{
    int card = 0;
    char *output = NULL;
    char *modename = NULL;
    bool printinfo = false;
    int c;

    for (;;) {
        c = getopt(argc, argv, "hic:");
        if (c == -1)
            break;
        switch (c) {
        case 'c':
            card = atoi(optarg);
            break;
        case 'i':
            printinfo = true;
            break;
        case 'h':
            usage(stdout);
            exit(0);
        default:
            usage(stderr);
            exit(1);
        }
    }

    drm_init_dev(card, output, modename, false);

    if (printinfo) {
        virtio_print_caps();
    }

    drm_fini_dev();
    return 0;
}
