#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

/* ------------------------------------------------------------------ */

/* device */
static int fd;
static drmModeConnector *conn = NULL;
static drmModeEncoder *enc = NULL;
static drmModeCrtc *scrtc = NULL;

/* dumb fb */
static struct drm_mode_create_dumb creq;
static uint32_t fb_id;
static uint8_t *fbmem;

static void drm_init_dev(int devnr, bool need_dumb, bool need_master)
{
    drmModeRes *res;
    char dev[64];
    int i, rc;
    uint64_t has_dumb;

    /* open device */
    snprintf(dev, sizeof(dev), DRM_DEV_NAME, DRM_DIR_NAME, devnr);
    fd = open(dev, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", dev, strerror(errno));
        exit(1);
    }

    if (need_dumb) {
        rc = drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb);
        if (rc < 0 || !has_dumb) {
            fprintf(stderr, "no dumb buffer support\n");
            exit(1);
        }
    }
    if (need_master) {
        rc = drmSetMaster(fd);
        if (rc < 0 || !has_dumb) {
            fprintf(stderr, "drmSetMaster() failed, X11 running?\n");
            exit(1);
        }
    }

    /* find connector (using first for now) */
    res = drmModeGetResources(fd);
    if (res == NULL) {
        fprintf(stderr, "drmModeGetResources() failed\n");
        exit(1);
    }
    for (i = 0; i < res->count_connectors; i++) {
        conn = drmModeGetConnector(fd, res->connectors[i]);
        if (conn &&
            (conn->connection == DRM_MODE_CONNECTED) &&
            conn->count_modes)
            break;
        drmModeFreeConnector(conn);
        conn = NULL;
    }
    if (!conn) {
        fprintf(stderr, "no usable connector found\n");
        exit(1);
    }
    enc = drmModeGetEncoder(fd, conn->encoder_id);
    if (enc == NULL) {
        fprintf(stderr, "drmModeGetEncoder() failed\n");
        exit(1);
    }

    /* save crtc */
    scrtc = drmModeGetCrtc(fd, enc->crtc_id);
}

static void drm_fini_dev(void)
{
    /* restore crtc */
    if (scrtc) {
        drmModeSetCrtc(fd, scrtc->crtc_id, scrtc->buffer_id, scrtc->x, scrtc->y,
                       &conn->connector_id, 1, &scrtc->mode);
    }
}

static void drm_init_dumb_fb(void)
{
    struct drm_mode_map_dumb mreq;
    int rc;

    /* create framebuffer */
    memset(&creq, 0, sizeof(creq));
    creq.width = conn->modes[0].hdisplay;
    creq.height = conn->modes[0].vdisplay;
    creq.bpp = 32;
    rc = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    if (rc < 0) {
        fprintf(stderr, "DRM_IOCTL_MODE_CREATE_DUMB: %s\n", strerror(errno));
        exit(1);
    }
    rc = drmModeAddFB(fd, creq.width, creq.height, 24, 32, creq.pitch,
                      creq.handle, &fb_id);
    if (rc < 0) {
        fprintf(stderr, "drmModeAddFB() failed\n");
        exit(1);
    }

    /* map framebuffer */
    memset(&mreq, 0, sizeof(mreq));
    mreq.handle = creq.handle;
    rc = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
    if (rc < 0) {
        fprintf(stderr, "DRM_IOCTL_MODE_MAP_DUMB: %s\n", strerror(errno));
        exit(1);
    }
    fbmem = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
    if (fbmem == MAP_FAILED) {
        fprintf(stderr, "framebuffer mmap: %s\n", strerror(errno));
        exit(1);
    }
}

static void drm_draw_dumb_fb(void)
{
    int x, y;
    uint8_t *ptr;

    for (y = 0; y < creq.height; y++) {
        for (x = 0; x < creq.width; x++) {
            ptr = fbmem + creq.pitch * y + 4 * x;
            ptr[0] = 0;         /* blue  */
            ptr[1] = x & 0xff;  /* green */
            ptr[2] = y & 0xff;  /* red   */
            ptr[3] = 0;         /* alpha */
        }
    }
    drmModeDirtyFB(fd, fb_id, 0, 0);
}

static void drm_show_dumb_fb(void)
{
    int rc;

    rc = drmModeSetCrtc(fd, enc->crtc_id, fb_id, 0, 0,
                        &conn->connector_id, 1,
                        &conn->modes[0]);
    if (rc < 0) {
        fprintf(stderr, "drmModeSetCrtc() failed\n");
        exit (1);
    }
}

/* ------------------------------------------------------------------ */

static void usage(FILE *fp)
{
    fprintf(fp,
            "\n"
            "usage: drmtest [ options ]\n"
            "\n"
            "options:\n"
            "  -h         print this\n"
            "  -c <nr>    pick card\n"
            "\n");
}

int main(int argc, char **argv)
{
    int card = 0;
    int c;

    for (;;) {
        c = getopt(argc, argv, "hc:");
        if (c == -1)
            break;
        switch (c) {
        case 'c':
            card = atoi(optarg);
            break;
        case 'h':
            usage(stdout);
            exit(0);
        default:
            usage(stderr);
            exit(1);
        }
    }

    drm_init_dev(card, true, true);
    drm_init_dumb_fb();
    drm_draw_dumb_fb();
    drm_show_dumb_fb();
    sleep(3);
    drm_fini_dev();
    return 0;
}
