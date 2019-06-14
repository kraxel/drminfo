#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <libdrm/drm_fourcc.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <gbm.h>
#include <epoxy/gl.h>
#include <epoxy/egl.h>

#include <cairo.h>
#include <pixman.h>

#include "ttytools.h"
#include "logind.h"
#include "drmtools.h"
#include "drm-lease.h"
#include "render.h"
#include "image.h"

/* ------------------------------------------------------------------ */

/* dumb fb */
static struct drm_mode_create_dumb creq;
static const struct fbformat *fmt = NULL;
static uint8_t *fbmem;

/* cairo + pixman */
static cairo_surface_t *cs;
static pixman_image_t *pxcs;
static pixman_image_t *pxfb;

/* user options */
static cairo_surface_t *image;

/* ------------------------------------------------------------------ */

static void drm_draw(bool autotest)
{
    char name[64];
    char info1[80], info2[80], info3[80];
    cairo_t *cr;

    snprintf(info1, sizeof(info1), "mode: %dx%d",
             drm_mode->hdisplay, drm_mode->vdisplay);

    if (fmt->fourcc) {
        snprintf(info2, sizeof(info2),
                 "bpp %d, fourcc %c%c%c%c (ADDFB2)",
                 fmt->bpp,
                 (fmt->fourcc >>  0) & 0xff,
                 (fmt->fourcc >>  8) & 0xff,
                 (fmt->fourcc >> 16) & 0xff,
                 (fmt->fourcc >> 24) & 0xff);
    } else {
        snprintf(info2, sizeof(info2),
                 "bpp %d, depth %d (legacy ADDFB)",
                 fmt->bpp, fmt->depth);
    }

    drm_conn_name(drm_conn, name, sizeof(name));
    snprintf(info3, sizeof(info3), "drm driver: %s, output %.10s",
             version->name, name);

    cr = cairo_create(cs);
    if (image) {
        render_image(cr, drm_mode->hdisplay, drm_mode->vdisplay, image);
    } else {
        render_test(cr, drm_mode->hdisplay, drm_mode->vdisplay, info1, info2,
                    autotest ? NULL : info3);
    }
    cairo_destroy(cr);

    if (pxcs && pxfb) {
        pixman_image_composite(PIXMAN_OP_SRC, pxcs, NULL, pxfb,
                               0, 0,
                               0, 0,
                               0, 0,
                               drm_mode->hdisplay, drm_mode->vdisplay);
    }
}

/* ------------------------------------------------------------------ */

static void drm_init_dumb_fb(bool use_pixman)
{
    struct drm_mode_map_dumb mreq;
    uint32_t zero = 0;
    int rc;

    /* create framebuffer */
    memset(&creq, 0, sizeof(creq));
    creq.width = drm_mode->hdisplay;
    creq.height = drm_mode->vdisplay;
    creq.bpp = fmt->bpp;
    rc = drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    if (rc < 0) {
        fprintf(stderr, "DRM_IOCTL_MODE_CREATE_DUMB: %s\n", strerror(errno));
        exit(1);
    }

    if (fmt->fourcc) {
        rc = drmModeAddFB2(drm_fd, creq.width, creq.height, fmt->fourcc,
                           &creq.handle, &creq.pitch, &zero,
                           &fb_id, 0);
        if (rc < 0) {
            fprintf(stderr, "drmModeAddFB2() failed (fourcc %c%c%c%c)\n",
                    (fmt->fourcc >>  0) & 0xff,
                    (fmt->fourcc >>  8) & 0xff,
                    (fmt->fourcc >> 16) & 0xff,
                    (fmt->fourcc >> 24) & 0xff);
            exit(1);
        }
    } else {
        rc = drmModeAddFB(drm_fd, creq.width, creq.height, fmt->depth, fmt->bpp,
                          creq.pitch, creq.handle, &fb_id);
        if (rc < 0) {
            fprintf(stderr, "drmModeAddFB() failed (bpp %d, depth %d)\n",
                    fmt->bpp, fmt->depth);
            exit(1);
        }
    }

    /* map framebuffer */
    memset(&mreq, 0, sizeof(mreq));
    mreq.handle = creq.handle;
    rc = drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
    if (rc < 0) {
        fprintf(stderr, "DRM_IOCTL_MODE_MAP_DUMB: %s\n", strerror(errno));
        exit(1);
    }
    fbmem = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd, mreq.offset);
    if (fbmem == MAP_FAILED) {
        fprintf(stderr, "framebuffer mmap: %s\n", strerror(errno));
        exit(1);
    }

    if (use_pixman) {
        pxfb = pixman_image_create_bits(fmt->pixman,
                                        creq.width,
                                        creq.height,
                                        (void*)fbmem,
                                        creq.pitch);
        pxcs = pixman_image_create_bits(PIXMAN_x2r10g10b10,
                                        creq.width,
                                        creq.height,
                                        NULL, 0);
        cs = cairo_image_surface_create_for_data((void*)pixman_image_get_data(pxcs),
                                                 CAIRO_FORMAT_RGB30,
                                                 creq.width,
                                                 creq.height,
                                                 pixman_image_get_stride(pxcs));
    } else {
        cs = cairo_image_surface_create_for_data(fbmem,
                                                 fmt->cairo,
                                                 creq.width,
                                                 creq.height,
                                                 creq.pitch);
    }
}

static void drm_draw_dumb_fb(bool autotest)
{
    drm_draw(autotest);
    drmModeDirtyFB(drm_fd, fb_id, 0, 0);
}

/* ------------------------------------------------------------------ */

static void usage(FILE *fp)
{
    fprintf(fp,
            "\n"
            "usage: drmtest [ options ]\n"
            "\n"
            "options:\n"
            "  -h           print this\n"
            "  -p           pixman mode\n"
            "  -a           autotest mode (don't print hardware info)\n"
            "  -c <nr>      pick card\n"
            "  -o <name>    pick output\n"
            "  -s <secs>    set sleep time (default: 60)\n"
            "  -i <file>    load and display image <file>\n"
            "  -f <fmt>     pick framebuffer format\n"
            "  -m <mode>    pick video mode format\n"
            "  -L <output>  get a drm lease for output\n"
            "\n");
}

int main(int argc, char **argv)
{
    int card = 0;
    int secs = 60;
    int lease_fd = -1;
    char *output = NULL;
    char *format = NULL;
    char *modename = NULL;
    char buf[32];
    bool autotest = false;
    bool pixman = false;
    int c,i;

    for (;;) {
        c = getopt(argc, argv, "hpaL:c:s:o:i:f:m:");
        if (c == -1)
            break;
        switch (c) {
        case 'p':
            pixman = true;
            break;
        case 'a':
            autotest = true;
            break;
        case 'c':
            card = atoi(optarg);
            break;
        case 's':
            secs = atoi(optarg);
            break;
        case 'i':
            image = load_image(optarg);
            break;
        case 'o':
            output = optarg;
            break;
        case 'f':
            format = optarg;
            break;
        case 'm':
            modename = optarg;
            break;
        case 'L':
            lease_fd = drm_lease(optarg);
            break;
        case 'h':
            usage(stdout);
            exit(0);
        default:
            usage(stderr);
            exit(1);
        }
    }

    if (format) {
        for (i = 0; i < fmtcnt; i++) {
            if (strcmp(format, fmts[i].name) == 0) {
                fmt = &fmts[i];
            }
        }
        if (!fmt) {
            fprintf(stderr, "unknown format %s, valid choices are:\n", format);
            for (i = 0; i < fmtcnt; i++) {
                if (pixman) {
                    if (fmts[i].pixman == 0)
                        continue;
                } else {
                    if (fmts[i].cairo == CAIRO_FORMAT_INVALID)
                        continue;
                }
                drm_print_format(stderr, &fmts[i], 4, false, false);
            }
            exit(1);
        }
    }

    logind_init();
    drm_init_dev(card, output, modename, false, lease_fd);

    if (!fmt) {
        /* find first supported in list */
        for (i = 0; i < fmtcnt; i++) {
            if (pixman) {
                if (fmts[i].pixman == 0)
                    continue;
            } else {
                if (fmts[i].cairo == CAIRO_FORMAT_INVALID)
                    continue;
            }
            if (!drm_probe_format_fb(drm_fd, &fmts[i]))
                continue;
            if (!drm_probe_format_primary(&fmts[i]))
                continue;
            fmt = &fmts[i];
            break;
        }
        if (!fmt) {
            drm_fini_dev();
            fprintf(stderr, "No drm format (with cairo support) found.\n");
            exit(1);
        }
    }

    if (fmt->cairo == CAIRO_FORMAT_INVALID) {
        if (fmt->pixman) {
            fprintf(stderr, "format %s not supported by cairo, enabling pixman mode\n",
                    fmt->name);
            pixman = true;
        } else {
            fprintf(stderr, "format %s not supported by cairo or pixman\n",
                    fmt->name);
            exit(1);
        }
    }

    drm_init_dumb_fb(pixman);
    drm_draw_dumb_fb(autotest);
    drm_show_fb();

    if (autotest)
        fprintf(stdout, "---ok---\n");
    tty_raw();
    kbd_wait(secs);
    read(0, buf, sizeof(buf));
    tty_restore();

    drm_fini_dev();
    logind_fini();

    return 0;
}
