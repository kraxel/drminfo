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
#include <drm/drm_fourcc.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cairo.h>
#include <pixman.h>

#include "drmtools.h"

/* ------------------------------------------------------------------ */

static void drm_info_conn(int fd, drmModeConnector *conn)
{
    drmModeEncoder *enc;
    drmModeCrtc *crtc;
    char name[64];
    int e, c, m;

    drm_conn_name(conn, name, sizeof(name));
    fprintf(stdout, "%s, %s\n",
            name, drm_connector_mode_name(conn->connection));

    for (e = 0; e < conn->count_encoders; e++) {
        enc = drmModeGetEncoder(fd, conn->encoders[e]);
        if (!enc)
            continue;
        fprintf(stdout, "    encoder: %s",
                drm_encoder_type_name(enc->encoder_type));
        if (enc->encoder_id == conn->encoder_id)
            fprintf(stdout, ", active");
        if (enc->crtc_id) {
            crtc = drmModeGetCrtc(fd, enc->crtc_id);
            if (crtc) {
                if (crtc->x || crtc->y) {
                    fprintf(stdout, ", %dx%d+%d+%d",
                            crtc->width, crtc->height, crtc->x, crtc->y);
                } else {
                    fprintf(stdout, ", %dx%d",
                            crtc->width, crtc->height);
                }
            }
        }
        fprintf(stdout, "\n");
        drmModeFreeEncoder(enc);
    }

    c = 1;
    for (m = 0; m < conn->count_modes; m++) {
        if (m+1 < conn->count_modes &&
            conn->modes[m].hdisplay == conn->modes[m+1].hdisplay &&
            conn->modes[m].vdisplay == conn->modes[m+1].vdisplay) {
            c++;
        } else {
            fprintf(stdout, "    mode: %dx%d",
                    conn->modes[m].hdisplay,
                    conn->modes[m].vdisplay);
            if (c > 1) {
                fprintf(stdout, " (%dx)", c);
            }
            fprintf(stdout, "\n");
            c = 1;
        };
    }
}

static void drm_info_conns(int fd)
{
    drmModeConnector *conn;
    drmModeRes *res;
    int i;

    res = drmModeGetResources(fd);
    if (res == NULL) {
        fprintf(stderr, "drmModeGetResources() failed\n");
        exit(1);
    }

    for (i = 0; i < res->count_connectors; i++) {
        conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn)
            continue;

        drm_info_conn(fd, conn);
        drmModeFreeConnector(conn);
        fprintf(stdout, "\n");
    }
}

static void drm_info_plane(int fd, drmModePlane *plane)
{
    int i;

    fprintf(stdout, "plane: %d, crtc: %d, fb: %d\n",
            plane->plane_id, plane->crtc_id, plane->fb_id);

    fprintf(stdout, "    formats:");
    for (i = 0; i < plane->count_formats; i++)
        fprintf(stdout, " %c%c%c%c",
                (plane->formats[i] >>  0) & 0xff,
                (plane->formats[i] >>  8) & 0xff,
                (plane->formats[i] >> 16) & 0xff,
                (plane->formats[i] >> 24) & 0xff);
    fprintf(stdout, "\n");
}

static void drm_info_planes(int fd)
{
    drmModePlaneRes *pres;
    drmModePlane *plane;
    int i;

    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    pres = drmModeGetPlaneResources(fd);
    if (pres == NULL) {
        fprintf(stderr, "drmModeGetPlaneResources() failed\n");
        exit(1);
    }

    for (i = 0; i < pres->count_planes; i++) {
        plane = drmModeGetPlane(fd, pres->planes[i]);
        if (!plane)
            continue;

        drm_info_plane(fd, plane);
        drmModeFreePlane(plane);
        fprintf(stdout, "\n");
    }
}

static void drm_info_fmts(int fd)
{
    int i;

    fprintf(stdout, "framebuffer formats\n");
    for (i = 0; i < fmtcnt; i++) {
        if (!drm_probe_format(fd, &fmts[i]))
            continue;
        drm_print_format(stdout, &fmts[i], 4, true);
    }
    fprintf(stdout, "\n");
}

static int drm_open(int devnr)
{
    char dev[64];
    int fd;

    snprintf(dev, sizeof(dev), DRM_DEV_NAME, DRM_DIR_NAME, devnr);
    fd = open(dev, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", dev, strerror(errno));
        exit(1);
    }
    return fd;
}

static void drm_info_all(int fd)
{
    char *busid;

    busid = drmGetBusid(fd);
    if (busid) {
        fprintf(stdout, "busid: \"%s\"\n", busid);
    }
    fprintf(stdout, "\n");

    drm_info_conns(fd);
    drm_info_planes(fd);
    drm_info_fmts(fd);
}

static void list_formats(FILE *fp)
{
    int i;

    fprintf(stdout, "all known framebuffer formats (rgb + packed yuv)\n");
    drm_print_format_hdr(stdout, 0, true);
    for (i = 0; i < fmtcnt; i++) {
        drm_print_format(stdout, &fmts[i], 0, true);
    }
    fprintf(stdout, "\n");
}

/* ------------------------------------------------------------------ */

static void usage(FILE *fp)
{
    fprintf(fp,
            "\n"
            "usage: drminfo [ options ]\n"
            "\n"
            "options:\n"
            "  -h         print this\n"
            "  -c <nr>    pick card\n"
            "  -l         print all known formats\n"
            "\n");
}

int main(int argc, char **argv)
{
    int card = 0;
    int c, fd;

    for (;;) {
        c = getopt(argc, argv, "hlc:");
        if (c == -1)
            break;
        switch (c) {
        case 'c':
            card = atoi(optarg);
            break;
        case 'l':
            list_formats(stdout);
            exit(0);
        case 'h':
            usage(stdout);
            exit(0);
        default:
            usage(stderr);
            exit(1);
        }
    }

    fd = drm_open(card);
    drm_info_all(fd);
    return 0;
}
