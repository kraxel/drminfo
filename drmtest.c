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
#include "drmtools.h"
#include "render.h"
#include "image.h"

/* ------------------------------------------------------------------ */

/* dumb fb */
static struct drm_mode_create_dumb creq;
static const struct fbformat *fmt = NULL;
static uint8_t *fbmem;

/* cairo */
static cairo_surface_t *cs;

/* user options */
static cairo_surface_t *image;

/* ------------------------------------------------------------------ */

static void drm_draw(void)
{
    char name[64];
    char info1[80], info2[80], info3[80];
    cairo_t *cr;

    snprintf(info1, sizeof(info1), "drm driver: %s, v%d.%d.%d (%s)",
             version->name, version->version_major, version->version_minor,
             version->version_patchlevel, version->desc);

    drm_conn_name(conn, name, sizeof(name));
    snprintf(info2, sizeof(info2), "%dx%d, output %s",
             mode->hdisplay, mode->vdisplay, name);

    if (fmt->fourcc) {
        snprintf(info3, sizeof(info3),
                 "dumb drm buffer, bpp %d, fourcc %c%c%c%c (ADDFB2)",
                 fmt->bpp,
                 (fmt->fourcc >>  0) & 0xff,
                 (fmt->fourcc >>  8) & 0xff,
                 (fmt->fourcc >> 16) & 0xff,
                 (fmt->fourcc >> 24) & 0xff);
    } else {
        snprintf(info3, sizeof(info3),
                 "dumb drm buffer, bpp %d, depth %d (legacy ADDFB)",
                 fmt->bpp, fmt->depth);
    }

    cr = cairo_create(cs);
    if (image) {
        render_image(cr, mode->hdisplay, mode->vdisplay, image);
    } else {
        render_test(cr, mode->hdisplay, mode->vdisplay, info1, info2, info3);
    }
    cairo_destroy(cr);
}

/* ------------------------------------------------------------------ */

static void drm_init_dumb_fb(void)
{
    struct drm_mode_map_dumb mreq;
    uint32_t zero = 0;
    int rc;

    /* create framebuffer */
    memset(&creq, 0, sizeof(creq));
    creq.width = mode->hdisplay;
    creq.height = mode->vdisplay;
    creq.bpp = fmt->bpp;
    rc = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    if (rc < 0) {
        fprintf(stderr, "DRM_IOCTL_MODE_CREATE_DUMB: %s\n", strerror(errno));
        exit(1);
    }

    if (fmt->fourcc) {
        rc = drmModeAddFB2(fd, creq.width, creq.height, fmt->fourcc,
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
        rc = drmModeAddFB(fd, creq.width, creq.height, fmt->depth, fmt->bpp,
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

    cs = cairo_image_surface_create_for_data(fbmem,
                                             fmt->cairo,
                                             creq.width,
                                             creq.height,
                                             creq.pitch);
}

static void drm_draw_dumb_fb(void)
{
    drm_draw();
    drmModeDirtyFB(fd, fb_id, 0, 0);
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
            "  -o <name>  pick output\n"
            "  -s <secs>  set sleep time\n"
            "  -i <file>  load and display image <file>\n"
            "  -f <fmt>   pick framebuffer format\n"
            "  -m <mode>  pick video mode format\n"
            "\n");
}

int main(int argc, char **argv)
{
    int card = 0;
    int secs = 60;
    char *output = NULL;
    char *format = NULL;
    char *modename = NULL;
    char buf[32];
    int c,i;

    for (;;) {
        c = getopt(argc, argv, "hc:s:o:i:f:m:");
        if (c == -1)
            break;
        switch (c) {
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
                if (fmts[i].cairo == CAIRO_FORMAT_INVALID)
                    continue;
                drm_print_format(stderr, &fmts[i], 4, false);
            }
            exit(1);
        }
    }

    drm_init_dev(card, output, modename, false);

    if (!fmt) {
        /* find first supported in list */
        for (i = 0; i < fmtcnt; i++) {
            if (fmts[i].cairo == CAIRO_FORMAT_INVALID)
                continue;
            if (!drm_probe_format(fd, &fmts[i]))
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

    drm_init_dumb_fb();
    drm_draw_dumb_fb();
    drm_show_fb();

    tty_raw();
    kbd_wait(secs);
    read(0, buf, sizeof(buf));
    tty_restore();

    drm_fini_dev();
    return 0;
}
