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

#include "drmtools.h"

/* ------------------------------------------------------------------ */

static void drm_info(int devnr)
{
    drmModeConnector *conn;
    drmModeEncoder *enc;
    drmModeCrtc *crtc;
    drmModeRes *res;
    char dev[64], *busid;
    char name[64];
    int fd, i, m, c, e;

    snprintf(dev, sizeof(dev), DRM_DEV_NAME, DRM_DIR_NAME, devnr);
    fd = open(dev, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", dev, strerror(errno));
        exit(1);
    }

    busid = drmGetBusid(fd);
    if (busid) {
        fprintf(stdout, "busid: \"%s\"\n", busid);
    }
    fprintf(stdout, "\n");

    res = drmModeGetResources(fd);
    if (res == NULL) {
        fprintf(stderr, "drmModeGetResources() failed\n");
        exit(1);
    }

    for (i = 0; i < res->count_connectors; i++) {
        conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn)
            continue;

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

        drmModeFreeConnector(conn);
        fprintf(stdout, "\n");
    }

    fprintf(stdout, "legacy formats\n");
    drm_probe_format(fd, 32, 24, 0, true);
    drm_probe_format(fd, 32, 30, 0, true);
    drm_probe_format(fd, 24, 24, 0, true);
    drm_probe_format(fd, 16, 16, 0, true);
    drm_probe_format(fd, 16, 15, 0, true);
    fprintf(stdout, "\n");

    fprintf(stdout, "fourcc formats\n");
    drm_probe_format(fd, 32, 0, DRM_FORMAT_XRGB8888,    true);
    drm_probe_format(fd, 32, 0, DRM_FORMAT_XBGR8888,    true);
    drm_probe_format(fd, 32, 0, DRM_FORMAT_RGBX8888,    true);
    drm_probe_format(fd, 32, 0, DRM_FORMAT_BGRX8888,    true);
    drm_probe_format(fd, 32, 0, DRM_FORMAT_ARGB8888,    true);
    drm_probe_format(fd, 32, 0, DRM_FORMAT_ABGR8888,    true);
    drm_probe_format(fd, 32, 0, DRM_FORMAT_RGBA8888,    true);
    drm_probe_format(fd, 32, 0, DRM_FORMAT_BGRA8888,    true);
    drm_probe_format(fd, 32, 0, DRM_FORMAT_XRGB2101010, true);
    drm_probe_format(fd, 32, 0, DRM_FORMAT_XBGR2101010, true);
    drm_probe_format(fd, 32, 0, DRM_FORMAT_RGBX1010102, true);
    drm_probe_format(fd, 32, 0, DRM_FORMAT_BGRX1010102, true);

    drm_probe_format(fd, 24, 0, DRM_FORMAT_RGB888,      true);
    drm_probe_format(fd, 24, 0, DRM_FORMAT_BGR888,      true);

    drm_probe_format(fd, 16, 0, DRM_FORMAT_XRGB4444,    true);
    drm_probe_format(fd, 16, 0, DRM_FORMAT_XBGR4444,    true);
    drm_probe_format(fd, 16, 0, DRM_FORMAT_RGBX4444,    true);
    drm_probe_format(fd, 16, 0, DRM_FORMAT_BGRX4444,    true);
    drm_probe_format(fd, 16, 0, DRM_FORMAT_ARGB4444,    true);
    drm_probe_format(fd, 16, 0, DRM_FORMAT_ABGR4444,    true);
    drm_probe_format(fd, 16, 0, DRM_FORMAT_RGBA4444,    true);
    drm_probe_format(fd, 16, 0, DRM_FORMAT_BGRA4444,    true);
    drm_probe_format(fd, 16, 0, DRM_FORMAT_XRGB1555,    true);
    drm_probe_format(fd, 16, 0, DRM_FORMAT_XBGR1555,    true);
    drm_probe_format(fd, 16, 0, DRM_FORMAT_RGBX5551,    true);
    drm_probe_format(fd, 16, 0, DRM_FORMAT_BGRX5551,    true);
    drm_probe_format(fd, 16, 0, DRM_FORMAT_ARGB1555,    true);
    drm_probe_format(fd, 16, 0, DRM_FORMAT_ABGR1555,    true);
    drm_probe_format(fd, 16, 0, DRM_FORMAT_RGBA5551,    true);
    drm_probe_format(fd, 16, 0, DRM_FORMAT_BGRA5551,    true);
    drm_probe_format(fd, 16, 0, DRM_FORMAT_RGB565,      true);
    drm_probe_format(fd, 16, 0, DRM_FORMAT_BGR565,      true);
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

    drm_info(card);
    return 0;
}
