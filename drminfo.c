#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>

#include <sys/ioctl.h>

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
    char dev[64];
    int fd, i, m, c, e;

    snprintf(dev, sizeof(dev), DRM_DEV_NAME, DRM_DIR_NAME, devnr);
    fd = open(dev, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", dev, strerror(errno));
        exit(1);
    }

    res = drmModeGetResources(fd);
    if (res == NULL) {
        fprintf(stderr, "drmModeGetResources() failed\n");
        exit(1);
    }

    for (i = 0; i < res->count_connectors; i++) {
        conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn)
            continue;

        fprintf(stdout, "id %d: %s-%d, %s\n",
                conn->connector_id,
                drm_connector_type_name(conn->connector_type),
                conn->connector_type_id,
                drm_connector_mode_name(conn->connection));

        if (conn->count_encoders) {
            fprintf(stdout, "    %d encoders\n",
                    conn->count_encoders);
            for (e = 0; e < conn->count_encoders; e++) {
                enc = drmModeGetEncoder(fd, conn->encoders[e]);
                if (!enc)
                    continue;
                fprintf(stdout, "        id %d: %s",
                        enc->encoder_id,
                        drm_encoder_type_name(enc->encoder_type));
                if (enc->encoder_id == conn->encoder_id)
                    fprintf(stdout, ", active");
                if (enc->crtc_id) {
                    crtc = drmModeGetCrtc(fd, enc->crtc_id);
                    if (crtc) {
                        fprintf(stdout, ", crtc %d (%dx%d)",
                                crtc->crtc_id,
                                crtc->width, crtc->height);
                    }
                }
                fprintf(stdout, "\n");
                drmModeFreeEncoder(enc);
            }
        }

        if (conn->count_modes) {
            c = 1;
            fprintf(stdout, "    %d modes\n", conn->count_modes);
            for (m = 0; m < conn->count_modes; m++) {
                if (m+1 < conn->count_modes &&
                    conn->modes[m].hdisplay == conn->modes[m+1].hdisplay &&
                    conn->modes[m].vdisplay == conn->modes[m+1].vdisplay) {
                    c++;
                } else {
                    fprintf(stdout, "        %dx%d",
                            conn->modes[m].hdisplay,
                            conn->modes[m].vdisplay);
                    if (c > 1) {
                        fprintf(stdout, " (%dx)", c);
                    }
                    fprintf(stdout, "\n");
                    c = 1;
                };
            }
        } else {
            fprintf(stdout, "    no modes\n");
        }

        drmModeFreeConnector(conn);
        fprintf(stdout, "\n");
    }
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
