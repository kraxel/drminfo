#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

#include <sys/ioctl.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "drmtools.h"

/* ------------------------------------------------------------------ */

static const char *conn_type[] = {
    [ DRM_MODE_CONNECTOR_Unknown      ] = "Unknown",
    [ DRM_MODE_CONNECTOR_VGA          ] = "VGA",
    [ DRM_MODE_CONNECTOR_DVII         ] = "DVII",
    [ DRM_MODE_CONNECTOR_DVID         ] = "DVID",
    [ DRM_MODE_CONNECTOR_DVIA         ] = "DVIA",
    [ DRM_MODE_CONNECTOR_Composite    ] = "Composite",
    [ DRM_MODE_CONNECTOR_SVIDEO       ] = "SVIDEO",
    [ DRM_MODE_CONNECTOR_LVDS         ] = "LVDS",
    [ DRM_MODE_CONNECTOR_Component    ] = "Component",
    [ DRM_MODE_CONNECTOR_9PinDIN      ] = "9PinDIN",
    [ DRM_MODE_CONNECTOR_DisplayPort  ] = "DisplayPort",
    [ DRM_MODE_CONNECTOR_HDMIA        ] = "HDMIA",
    [ DRM_MODE_CONNECTOR_HDMIB        ] = "HDMIB",
    [ DRM_MODE_CONNECTOR_TV           ] = "TV",
    [ DRM_MODE_CONNECTOR_eDP          ] = "eDP",
    [ DRM_MODE_CONNECTOR_VIRTUAL      ] = "VIRTUAL",
    [ DRM_MODE_CONNECTOR_DSI          ] = "DSI",
};

static const char *conn_mode[] = {
    [ DRM_MODE_CONNECTED              ] = "connected",
    [ DRM_MODE_DISCONNECTED           ] = "disconnected",
    [ DRM_MODE_UNKNOWNCONNECTION      ] = "unknown",
};

static const char *enc_type[] = {
    [ DRM_MODE_ENCODER_NONE           ] = "NONE",
    [ DRM_MODE_ENCODER_DAC            ] = "DAC",
    [ DRM_MODE_ENCODER_TMDS           ] = "TMDS",
    [ DRM_MODE_ENCODER_LVDS           ] = "LVDS",
    [ DRM_MODE_ENCODER_TVDAC          ] = "TVDAC",
    [ DRM_MODE_ENCODER_VIRTUAL        ] = "VIRTUAL",
    [ DRM_MODE_ENCODER_DSI            ] = "DSI",
};

static const char *enum2name(const char *names[], int len, int nr)
{
    if (nr >= len)
        return "???";
    if (!names[nr])
        return "???";
    return names[nr];
}

/* ------------------------------------------------------------------ */

const char *drm_connector_type_name(int nr)
{
    return enum2name(conn_type, sizeof(conn_type)/sizeof(conn_type[0]), nr);
}

const char *drm_connector_mode_name(int nr)
{
    return enum2name(conn_mode, sizeof(conn_mode)/sizeof(conn_mode[0]), nr);
}

const char *drm_encoder_type_name(int nr)
{
    return enum2name(enc_type, sizeof(enc_type)/sizeof(enc_type[0]), nr);
}

void drm_conn_name(drmModeConnector *conn, char *dest, int dlen)
{
    const char *type;

    if (conn->connector_type_id < sizeof(conn_type)/sizeof(conn_type[0]) &&
        conn_type[conn->connector_type]) {
        type = conn_type[conn->connector_type];
    } else {
        type = "unknown";
    }
    snprintf(dest, dlen, "%s-%d", type, conn->connector_type_id);
}

/* ------------------------------------------------------------------ */

bool drm_probe_format(int fd, uint32_t bpp, uint32_t depth, uint32_t fourcc, bool print)
{
    struct drm_mode_create_dumb cd;
    struct drm_mode_destroy_dumb dd;
    uint32_t zero = 0;
    uint32_t fb_id = 0;
    bool result = false;
    int rc;

    /* create dumb buffer */
    memset(&cd, 0, sizeof(cd));
    cd.width = 64;
    cd.height = 64;
    cd.bpp = bpp;
    rc = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &cd);
    if (rc < 0)
        goto done;

    /* create framebuffer */
    if (fourcc) {
        rc = drmModeAddFB2(fd, cd.width, cd.height, fourcc,
                           &cd.handle, &cd.pitch, &zero,
                           &fb_id, 0);
        if (rc == 0 && print)
            printf("    bpp %d, fourcc %c%c%c%c\n", cd.bpp,
                   (fourcc >>  0) & 0xff,
                   (fourcc >>  8) & 0xff,
                   (fourcc >> 16) & 0xff,
                   (fourcc >> 24) & 0xff);
    } else if (depth) {
        rc = drmModeAddFB(fd, cd.width, cd.height, depth, cd.bpp,
                          cd.pitch, cd.handle, &fb_id);
        if (rc == 0 && print)
            printf("    bpp %d, depth %d\n", cd.bpp, depth);
    } else {
        rc = -1;
    }

    if (rc == 0) {
        /* worked, yay! */
        result = true;
        drmModeRmFB(fd, fb_id);
    }
    dd.handle = cd.handle;
    drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dd);

done:
    return result;
}
