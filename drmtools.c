#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <endian.h>

#include <sys/ioctl.h>
#include <linux/virtio_gpu.h>
#include <libdrm/drm_fourcc.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cairo.h>
#include <pixman.h>

#include "drmtools.h"

/* ------------------------------------------------------------------ */

#define FOURCC_NAME(_fcc) {                     \
        (_fcc >>  0) & 0xff,                    \
        (_fcc >>  8) & 0xff,                    \
        (_fcc >> 16) & 0xff,                    \
        (_fcc >> 24) & 0xff,                    \
        0 }

#if __BYTE_ORDER == __LITTLE_ENDIAN
# define LE_BE(_le, _be) _le
#elif __BYTE_ORDER == __BIG_ENDIAN
# define LE_BE(_le, _be) _be
#endif

const struct fbformat fmts[] = {

    /*
     * -----------------------------------------------------------------
     * fourcc formats, little endian (DRM_IOCTL_MODE_ADDFB2)
     */

    /* --- 8 bpp, rgb --- */
    {
        .name   = FOURCC_NAME(DRM_FORMAT_RGB332),
        .fields = "R:G:B",
        .bits   = "3:3:2",
        .bpp    = 8,
        .fourcc = DRM_FORMAT_RGB332,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = PIXMAN_r3g3b2,
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_BGR233),
        .fields = "B:G:R",
        .bits   = "2:3:3",
        .bpp    = 8,
        .fourcc = DRM_FORMAT_BGR233,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = PIXMAN_b2g3r3,
    },

    /* -- 16 bpp, rgb --- */
    {
        .name   = FOURCC_NAME(DRM_FORMAT_XRGB4444),
        .fields = "x:R:G:B",
        .bits   = "4:4:4:4",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_XRGB4444,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_x4r4g4b4, 0),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_XBGR4444),
        .fields = "x:B:G:R",
        .bits   = "4:4:4:4",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_XBGR4444,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_x4b4g4r4, 0),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_RGBX4444),
        .fields = "R:G:B:x",
        .bits   = "4:4:4:4",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_RGBX4444,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = 0,
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_BGRX4444),
        .fields = "B:G:R:x",
        .bits   = "4:4:4:4",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_BGRX4444,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = 0,
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_ARGB4444),
        .fields = "A:R:G:B",
        .bits   = "4:4:4:4",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_ARGB4444,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_a4r4g4b4, 0),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_ABGR4444),
        .fields = "A:B:G:R",
        .bits   = "4:4:4:4",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_ABGR4444,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_a4b4g4r4, 0),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_RGBA4444),
        .fields = "R:G:B:A",
        .bits   = "4:4:4:4",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_RGBA4444,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = 0,
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_BGRA4444),
        .fields = "B:G:R:A",
        .bits   = "4:4:4:4",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_BGRA4444,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = 0,
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_XRGB1555),
        .fields = "x:R:G:B",
        .bits   = "1:5:5:5",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_XRGB1555,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_x1r5g5b5, 0),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_XBGR1555),
        .fields = "x:B:G:R",
        .bits   = "1:5:5:5",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_XBGR1555,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_x1b5g5r5, 0),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_RGBX5551),
        .fields = "R:G:B:x",
        .bits   = "5:5:5:1",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_RGBX5551,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = 0,
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_BGRX5551),
        .fields = "B:G:R:x",
        .bits   = "5:5:5:1",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_BGRX5551,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = 0,
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_ARGB1555),
        .fields = "A:R:G:B",
        .bits   = "1:5:5:5",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_ARGB1555,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_a1r5g5b5, 0),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_ABGR1555),
        .fields = "A:B:G:R",
        .bits   = "1:5:5:5",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_ABGR1555,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_a1b5g5r5, 0),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_RGBA5551),
        .fields = "R:G:B:A",
        .bits   = "5:5:5:1",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_RGBA5551,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = 0,
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_BGRA5551),
        .fields = "B:G:R:A",
        .bits   = "5:5:5:1",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_BGRA5551,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = 0,
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_RGB565),
        .fields = "R:G:B",
        .bits   = "5:6:5",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_RGB565,
        .cairo  = LE_BE(CAIRO_FORMAT_RGB16_565, CAIRO_FORMAT_INVALID),
        .pixman = LE_BE(PIXMAN_r5g6b5, 0),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_BGR565),
        .fields = "B:G:R",
        .bits   = "5:6:5",
        .bpp    = 16,
        .fourcc = DRM_FORMAT_BGR565,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_b5g6r5, 0),
    },

    /* --- 24 bpp, rgb --- */
    {
        .name   = FOURCC_NAME(DRM_FORMAT_RGB888),
        .fields = "R:G:B",
        .bits   = "8:8:8",
        .bpp    = 24,
        .fourcc = DRM_FORMAT_RGB888,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_r8g8b8, PIXMAN_b8g8r8),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_BGR888),
        .fields = "B:G:R",
        .bits   = "8:8:8",
        .bpp    = 24,
        .fourcc = DRM_FORMAT_BGR888,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_b8g8r8, PIXMAN_r8g8b8),
    },

    /* --- 32 bpp, rgb --- */
    {
        .name   = FOURCC_NAME(DRM_FORMAT_XRGB8888),
        .fields = "x:R:G:B",
        .bits   = "8:8:8:8",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_XRGB8888,
        .virtio = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM,
        .cairo  = LE_BE(CAIRO_FORMAT_RGB24, CAIRO_FORMAT_INVALID),
        .pixman = LE_BE(PIXMAN_x8r8g8b8, PIXMAN_b8g8r8x8),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_XBGR8888),
        .fields = "x:B:G:R",
        .bits   = "8:8:8:8",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_XBGR8888,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_x8b8g8r8, PIXMAN_r8g8b8x8),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_RGBX8888),
        .fields = "R:G:B:x",
        .bits   = "8:8:8:8",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_RGBX8888,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_r8g8b8x8, PIXMAN_x8b8g8r8),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_BGRX8888),
        .fields = "B:G:R:x",
        .bits   = "8:8:8:8",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_BGRX8888,
        .virtio = VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM,
        .cairo  = LE_BE(CAIRO_FORMAT_INVALID, CAIRO_FORMAT_RGB24),
        .pixman = LE_BE(PIXMAN_b8g8r8x8, PIXMAN_x8r8g8b8),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_ARGB8888),
        .fields = "A:R:G:B",
        .bits   = "8:8:8:8",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_ARGB8888,
        .virtio = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_a8r8g8b8, PIXMAN_b8g8r8a8),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_ABGR8888),
        .fields = "A:B:G:R",
        .bits   = "8:8:8:8",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_ABGR8888,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_a8b8g8r8, PIXMAN_r8g8b8a8),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_RGBA8888),
        .fields = "R:G:B:A",
        .bits   = "8:8:8:8",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_RGBA8888,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_r8g8b8a8, PIXMAN_a8b8g8r8),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_BGRA8888),
        .fields = "B:G:R:A",
        .bits   = "8:8:8:8",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_BGRA8888,
        .virtio = VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_b8g8r8a8, PIXMAN_a8r8g8b8),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_XRGB2101010),
        .fields = "x:R:G:B",
        .bits   = "2:10:10:10",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_XRGB2101010,
        .cairo  = LE_BE(CAIRO_FORMAT_RGB30, CAIRO_FORMAT_INVALID),
        .pixman = LE_BE(PIXMAN_x2r10g10b10, 0),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_XBGR2101010),
        .fields = "x:B:G:R",
        .bits   = "2:10:10:10",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_XBGR2101010,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_x2b10g10r10, 0),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_RGBX1010102),
        .fields = "R:G:B:x",
        .bits   = "10:10:10:2",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_RGBX1010102,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = 0,
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_BGRX1010102),
        .fields = "B:G:R:x",
        .bits   = "10:10:10:2",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_BGRX1010102,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = 0,
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_ARGB2101010),
        .fields = "A:R:G:B",
        .bits   = "2:10:10:10",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_ARGB2101010,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_a2r10g10b10, 0),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_ABGR2101010),
        .fields = "A:B:G:R",
        .bits   = "2:10:10:10",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_ABGR2101010,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = LE_BE(PIXMAN_a2b10g10r10, 0),
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_RGBA1010102),
        .fields = "R:G:B:A",
        .bits   = "10:10:10:2",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_RGBA1010102,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = 0,
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_BGRA1010102),
        .fields = "B:G:R:A",
        .bits   = "10:10:10:2",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_BGRA1010102,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = 0,
    },

    /* --- 32 bpp, yuv --- */
    {
        .name   = FOURCC_NAME(DRM_FORMAT_YUYV),
        .fields = "Cr0:Y1:Cb0:Y0",
        .bits   = "8:8:8:8",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_YUYV,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = 0,
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_YVYU),
        .fields = "Cb0:Y1:Cr0:Y0",
        .bits   = "8:8:8:8",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_YVYU,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = 0,
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_UYVY),
        .fields = "Y1:Cr0:Y0:Cb0",
        .bits   = "8:8:8:8",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_UYVY,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = 0,
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_VYUY),
        .fields = "Y1:Cb0:Y0:Cr0",
        .bits   = "8:8:8:8",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_VYUY,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = 0,
    },{
        .name   = FOURCC_NAME(DRM_FORMAT_AYUV),
        .fields = "A:Y:Cb:Cr",
        .bits   = "8:8:8:8",
        .bpp    = 32,
        .fourcc = DRM_FORMAT_AYUV,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = 0,
    },

    /*
     * -----------------------------------------------------------------
     * legacy formats, native endian (DRM_IOCTL_MODE_ADDFB)
     */
    {
        .name   = "24",
        .fields = "x:R:G:B",
        .bits   = "8:8:8:8",
        .bpp    = 32,
        .depth  = 24,
        .cairo  = CAIRO_FORMAT_RGB24,
        .pixman = PIXMAN_x8r8g8b8,
    },{
        .name   = "30",
        .fields = "x:R:G:B",
        .bits   = "2:10:10:10",
        .bpp    = 32,
        .depth  = 30,
        .cairo  = CAIRO_FORMAT_RGB30,
        .pixman = PIXMAN_x2b10g10r10,
    },{
        .name   = "15",
        .fields = "x:R:G:B",
        .bits   = "1:5:5:5",
        .bpp    = 16,
        .depth  = 15,
        .cairo  = CAIRO_FORMAT_INVALID,
        .pixman = PIXMAN_x1r5g5b5,
    },{
        .name   = "16",
        .fields = "R:G:B",
        .bits   = "5:6:5",
        .bpp    = 16,
        .depth  = 16,
        .cairo  = CAIRO_FORMAT_RGB16_565,
        .pixman = PIXMAN_r5g6b5,
    },
};
const uint32_t fmtcnt = sizeof(fmts)/sizeof(fmts[0]);

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

bool drm_probe_format(int fd, const struct fbformat *fmt)
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
    cd.bpp = fmt->bpp;
    rc = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &cd);
    if (rc < 0)
        goto done;

    /* create framebuffer */
    if (fmt->fourcc) {
        rc = drmModeAddFB2(fd, cd.width, cd.height, fmt->fourcc,
                           &cd.handle, &cd.pitch, &zero,
                           &fb_id, 0);
    } else if (fmt->depth) {
        rc = drmModeAddFB(fd, cd.width, cd.height, fmt->depth, cd.bpp,
                          cd.pitch, cd.handle, &fb_id);
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

void drm_print_format(FILE *fp, const struct fbformat *fmt,
                      int indent, bool libs)
{
    fprintf(fp, "%*s%-8s:  [%2d:0]  %-14s %-11s %-16s",
            indent, "",
            fmt->name, fmt->bpp - 1, fmt->fields, fmt->bits,
            fmt->fourcc
            ? "fourcc  le"
            : "legacy  cpu " LE_BE("(le)", "(be)"));
    if (libs) {
        fprintf(fp, "  %-6s %-5s",
                fmt->pixman                          ? "pixman" : "",
                (fmt->cairo != CAIRO_FORMAT_INVALID) ? "cairo"  : "");
    }
    fprintf(fp, "\n");
}

void drm_print_format_hdr(FILE *fp, int indent, bool libs)
{
    fprintf(fp, "%*s%-8s:  %-6s  %-14s %-11s %-16s",
            indent, "",
            "name", "bpp", "fields", "bits",
            "type    endian");
    if (libs) {
        fprintf(fp, "  lib support");
    }
    fprintf(fp, "\n");
}

/* ------------------------------------------------------------------ */

int fd;
uint32_t fb_id;
drmModeConnector *conn = NULL;
drmModeModeInfo *mode = NULL;
drmVersion *version = NULL;

static drmModeEncoder *enc = NULL;
static drmModeCrtc *scrtc = NULL;

void drm_init_dev(int devnr, const char *output,
                  const char *modename, bool need_dumb)
{
    drmModeRes *res;
    char dev[64];
    char name[64];
    char m[64];
    int i, rc;
    uint64_t has_dumb;

    /* open device */
    snprintf(dev, sizeof(dev), DRM_DEV_NAME, DRM_DIR_NAME, devnr);
    fd = open(dev, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", dev, strerror(errno));
        exit(1);
    }
    version = drmGetVersion(fd);

    if (need_dumb) {
        rc = drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb);
        if (rc < 0 || !has_dumb) {
            fprintf(stderr, "no dumb buffer support\n");
            exit(1);
        }
    }
#if 0
    if (need_master) {
        rc = drmSetMaster(fd);
        if (rc < 0) {
            fprintf(stderr, "drmSetMaster() failed: %s\n",
                    strerror(errno));
            exit(1);
        }
    }
#endif

    /* find connector */
    res = drmModeGetResources(fd);
    if (res == NULL) {
        fprintf(stderr, "drmModeGetResources() failed\n");
        exit(1);
    }
    for (i = 0; i < res->count_connectors; i++) {
        conn = drmModeGetConnector(fd, res->connectors[i]);
        if (conn &&
            (conn->connection == DRM_MODE_CONNECTED) &&
            conn->count_modes) {
            if (output) {
                drm_conn_name(conn, name, sizeof(name));
                if (strcmp(name, output) == 0) {
                    break;
                }
            } else {
                break;
            }
        }
        drmModeFreeConnector(conn);
        conn = NULL;
    }
    if (!conn) {
        if (output) {
            fprintf(stderr, "drm: output %s not found or disconnected\n",
                    output);
        } else {
            fprintf(stderr, "drm: no usable output found\n");
        }
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

    if (modename) {
        for (i = 0; i < conn->count_modes; i++) {
            snprintf(m, sizeof(m), "%dx%d",
                     conn->modes[i].hdisplay,
                     conn->modes[i].vdisplay);
            if (strcmp(m, modename) == 0) {
                fprintf(stderr, "Using mode %s\n", modename);
                mode = &conn->modes[i];
                break;
            }
        }
    }
    if (!mode) {
        mode = &conn->modes[0];
    }

    enc = drmModeGetEncoder(fd, conn->encoder_id);
    if (enc == NULL) {
        fprintf(stderr, "drmModeGetEncoder() failed\n");
        exit(1);
    }

    /* save crtc */
    scrtc = drmModeGetCrtc(fd, enc->crtc_id);
}

void drm_fini_dev(void)
{
    /* restore crtc */
    if (scrtc) {
        drmModeSetCrtc(fd, scrtc->crtc_id, scrtc->buffer_id, scrtc->x, scrtc->y,
                       &conn->connector_id, 1, &scrtc->mode);
    }
}

void drm_show_fb(void)
{
    int rc;

    rc = drmModeSetCrtc(fd, enc->crtc_id, fb_id, 0, 0,
                        &conn->connector_id, 1,
                        mode);
    if (rc < 0) {
        fprintf(stderr, "drmModeSetCrtc() failed: %s\n", strerror(errno));
        exit (1);
    }
}
