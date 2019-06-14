#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <assert.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/virtio_gpu.h>
#include <libdrm/drm_fourcc.h>
#include <libdrm/virtgpu_drm.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cairo.h>
#include <pixman.h>

#include "drmtools.h"
#include "logind.h"
#include "ttytools.h"
#include "render.h"

/* ------------------------------------------------------------------ */

#define ARRAY_SIZE(_x) (sizeof(_x)/sizeof(_x[0]))

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

static int virtio_get_cap(uint64_t cap, int *value)
{
    struct drm_virtgpu_getparam args;
    int rc;

    args.param = cap;
    args.value = (intptr_t)(value);
    rc = drmIoctl(drm_fd, DRM_IOCTL_VIRTGPU_GETPARAM, &args);
    if (rc != 0) {
        if (errno == EINVAL) {
            return -1;
        } else {
            fprintf(stderr, "ioctl DRM_IOCTL_VIRTGPU_GETPARAM(%" PRId64 "): %s\n",
                    cap, strerror(errno));
            exit(1);
        }
    }
    return 0;
}

static void virtio_print_caps(void)
{
    int i, rc, value;

    printf("virtio capabilities\n");
    for (i = 0; i < ARRAY_SIZE(virtio_caps); i++) {
        rc = virtio_get_cap(virtio_caps[i].cap, &value);
        if (rc == -1) {
            printf("    %-12s: not available\n", virtio_caps[i].name);
        } else {
            printf("    %-12s: %d\n", virtio_caps[i].name, value);
        }
    }
    fprintf(stdout, "\n");
}

static void virtio_list_format(void)
{
    int i;

    drm_print_format_hdr(stdout, 0, false, true);
    for (i = 0; i < fmtcnt; i++) {
        if (!fmts[i].virtio)
            continue;
        drm_print_format(stdout, fmts + i, 0, false, true);
    }
    fprintf(stdout, "\n");
}

static void virtio_check(int cardno)
{
    char syspath[128];
    char symlink[128];
    int len;

    snprintf(syspath, sizeof(syspath), "/sys/class/drm/card%d", cardno);
    len = readlink(syspath, symlink, sizeof(symlink));
    if (len < 0) {
        fprintf(stderr, "readlink %s: %s\n", syspath, strerror(errno));
        exit(1);
    }
    symlink[len] = 0;
    if (strstr(symlink, "/virtio") == NULL) {
        fprintf(stderr, "card%d: not a virtio-gpu device\n", cardno);
        exit(1);
    }
}

/* ------------------------------------------------------------------ */

static struct drm_virtgpu_resource_create create;
static uint8_t *fbmem;
static const struct fbformat *fmt;
static cairo_surface_t *cs;

static void virtio_init_fb(void)
{
    struct drm_virtgpu_resource_info info;
    struct drm_virtgpu_map map;
    uint32_t stride, zero = 0;
    int rc;

    /* create framebuffer */
    memset(&create, 0, sizeof(create));
    create.format = fmt->virtio;
    create.width  = drm_mode->hdisplay;
    create.height = drm_mode->vdisplay;

    create.target = 2;       /* ??? */
    create.bind = (1 << 1);  /* ??? */
    create.depth = 1;
    create.array_size = 1;

    stride = create.width * fmt->bpp / 8;
    create.size   = stride * create.height;
    rc = drmIoctl(drm_fd, DRM_IOCTL_VIRTGPU_RESOURCE_CREATE, &create);
    if (rc < 0) {
        fprintf(stderr, "DRM_IOCTL_VIRTGPU_RESOURCE_CREATE: %s\n",
                strerror(errno));
        exit(1);
    }

    memset(&info, 0, sizeof(info));
    info.bo_handle = create.bo_handle;
    rc = drmIoctl(drm_fd, DRM_IOCTL_VIRTGPU_RESOURCE_INFO, &info);
    if (rc < 0) {
        fprintf(stderr, "DRM_IOCTL_VIRTGPU_RESOURCE_INFO: %s\n",
                strerror(errno));
        exit(1);
    }

    memset(&map, 0, sizeof(map));
    map.handle = create.bo_handle;
    rc = drmIoctl(drm_fd, DRM_IOCTL_VIRTGPU_MAP, &map);
    if (rc < 0) {
        fprintf(stderr, "DRM_IOCTL_VIRTGPU_MAP: %s\n", strerror(errno));
        exit(1);
    }

    fbmem = mmap(0, info.size, PROT_READ | PROT_WRITE,
                 MAP_SHARED, drm_fd, map.offset);
    if (fbmem == MAP_FAILED) {
        fprintf(stderr, "framebuffer mmap: %s\n", strerror(errno));
        exit(1);
    }

    if (info.stride)
        stride = info.stride;
    rc = drmModeAddFB2(drm_fd, create.width, create.height, fmt->fourcc,
                       &create.bo_handle, &stride, &zero, &fb_id, 0);
    if (rc < 0) {
        fprintf(stderr, "drmModeAddFB2() failed: %s\n", strerror(errno));
        exit(1);
    }

    cs = cairo_image_surface_create_for_data(fbmem,
                                             fmt->cairo,
                                             create.width,
                                             create.height,
                                             stride);
}

static void virtio_draw(void)
{
    char info1[80], info2[80], info3[80];
    cairo_t *cr;

    snprintf(info1, sizeof(info1), "virtio-gpu");
    snprintf(info2, sizeof(info2), "%dx%d",
             drm_mode->hdisplay, drm_mode->vdisplay);
    snprintf(info3, sizeof(info3), "fourcc %c%c%c%c",
             (fmt->fourcc >>  0) & 0xff,
             (fmt->fourcc >>  8) & 0xff,
             (fmt->fourcc >> 16) & 0xff,
             (fmt->fourcc >> 24) & 0xff);

    cr = cairo_create(cs);
    render_test(cr, drm_mode->hdisplay, drm_mode->vdisplay, info1, info2, info3);
    cairo_destroy(cr);
}

static void virtio_transfer(void)
{
    struct drm_virtgpu_3d_transfer_to_host xfer;
    int rc;

    memset(&xfer, 0, sizeof(xfer));
    xfer.bo_handle = create.bo_handle;
    xfer.box.w = drm_mode->hdisplay;
    xfer.box.h = drm_mode->vdisplay;
    xfer.box.d = 1;
    rc = drmIoctl(drm_fd, DRM_IOCTL_VIRTGPU_TRANSFER_TO_HOST, &xfer);
    if (rc < 0) {
        fprintf(stderr, "DRM_IOCTL_VIRTGPU_TRANSFER_TO_HOST: %s\n",
                strerror(errno));
        exit(1);
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
            "  -a         autotest mode\n"
            "  -c <nr>    pick card\n"
            "  -s <secs>  set sleep time (default: 60)\n"
            "  -i         print device info\n"
            "  -l         list formats\n"
            "\n");
}

int main(int argc, char **argv)
{
    int card = 0;
    int secs = 60;
    char *output = NULL;
    char *modename = NULL;
    bool printinfo = false;
    bool listformat = false;
    bool autotest = false;
    char buf[32];
    int c, i;

    for (;;) {
        c = getopt(argc, argv, "hailc:s:");
        if (c == -1)
            break;
        switch (c) {
        case 'c':
            card = atoi(optarg);
            break;
        case 's':
            secs = atoi(optarg);
            break;
        case 'a':
            autotest = true;
            break;
        case 'i':
            printinfo = true;
            break;
        case 'l':
            listformat = true;
            break;
        case 'h':
            usage(stdout);
            exit(0);
        default:
            usage(stderr);
            exit(1);
        }
    }

    virtio_check(card);

    for (i = 0; i < fmtcnt; i++) {
        if (fmts[i].cairo == CAIRO_FORMAT_RGB24 &&
            fmts[i].virtio != 0) {
            fmt = &fmts[i];
        }
    }
    assert(fmt != NULL);

    logind_init();
    drm_init_dev(card, output, modename, false, -1);

    if (printinfo)
        virtio_print_caps();
    if (listformat)
        virtio_list_format();
    if (printinfo || listformat)
        goto done;

    virtio_init_fb();
    virtio_draw();
    virtio_transfer();
    drm_show_fb();

    if (autotest)
        fprintf(stdout, "---ok---\n");
    tty_raw();
    kbd_wait(secs);
    read(0, buf, sizeof(buf));
    tty_restore();

done:
    drm_fini_dev();
    logind_fini();
    return 0;
}
