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
#include "logind.h"

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

    /* --- 32 bpp, rgb --- */
    {
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
    },{
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

    /* -- 16 bpp, rgb --- */
    {
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
    },

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
    /*
     * Names are taken from the xorg modesetting driver, so the output
     * names are matching the ones ypu can see with xrandr.
     */
    [DRM_MODE_CONNECTOR_Unknown]     = "None",
    [DRM_MODE_CONNECTOR_VGA]         = "VGA",
    [DRM_MODE_CONNECTOR_DVII]        = "DVI-I",
    [DRM_MODE_CONNECTOR_DVID]        = "DVI-D",
    [DRM_MODE_CONNECTOR_DVIA]        = "DVI-A",
    [DRM_MODE_CONNECTOR_Composite]   = "Composite",
    [DRM_MODE_CONNECTOR_SVIDEO]      = "SVIDEO",
    [DRM_MODE_CONNECTOR_LVDS]        = "LVDS",
    [DRM_MODE_CONNECTOR_Component]   = "Component",
    [DRM_MODE_CONNECTOR_9PinDIN]     = "DIN",
    [DRM_MODE_CONNECTOR_DisplayPort] = "DP",
    [DRM_MODE_CONNECTOR_HDMIA]       = "HDMI",
    [DRM_MODE_CONNECTOR_HDMIB]       = "HDMI-B",
    [DRM_MODE_CONNECTOR_TV]          = "TV",
    [DRM_MODE_CONNECTOR_eDP]         = "eDP",
    [DRM_MODE_CONNECTOR_VIRTUAL]     = "Virtual",
    [DRM_MODE_CONNECTOR_DSI]         = "DSI",
    [DRM_MODE_CONNECTOR_DPI]         = "DPI",
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

    if (conn->connector_type < sizeof(conn_type)/sizeof(conn_type[0]) &&
        conn_type[conn->connector_type]) {
        type = conn_type[conn->connector_type];
    } else {
        type = "unknown";
    }
    snprintf(dest, dlen, "%s-%d", type, conn->connector_type_id);
}

/* ------------------------------------------------------------------ */

static drmModePlane *primary;
static drmModePlane *cursor;
static drmModePlane *overlay;

uint64_t drm_get_property_value(int fd, uint32_t id, uint32_t objtype,
                                const char *name)
{
   drmModeObjectProperties *props =
       drmModeObjectGetProperties(fd, id, objtype);
   drmModePropertyPtr prop;
   uint64_t value = 0;
   uint32_t i;

   for (i = 0; i < props->count_props; i++) {
       prop = drmModeGetProperty(fd, props->props[i]);
       if (!strcmp(prop->name, name))
           value = props->prop_values[i];
       drmModeFreeProperty(prop);
   }
   drmModeFreeObjectProperties(props);

   return value;
}

static bool drm_probe_format_plane(const drmModePlane *plane,
                                   const struct fbformat *fmt)
{
    int i;

    if (!plane)
        return false;
    for (i = 0; i < plane->count_formats; i++)
        if (plane->formats[i] == fmt->fourcc)
            return true;
    return false;
}

bool drm_probe_format_primary(const struct fbformat *fmt)
{
    return drm_probe_format_plane(primary, fmt);
}

bool drm_probe_format_cursor(const struct fbformat *fmt)
{
    return drm_probe_format_plane(cursor, fmt);
}

bool drm_probe_format_overlay(const struct fbformat *fmt)
{
    return drm_probe_format_plane(overlay, fmt);
}

void drm_plane_init(int fd)
{
    drmModePlaneRes *pres;
    drmModePlane *plane;
    uint64_t type;
    int i;

    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    pres = drmModeGetPlaneResources(fd);
    if (pres == NULL) {
        fprintf(stderr, "drmModeGetPlaneResources() failed\n");
        exit(1);
    }

    /* find plane */
    for (i = 0; i < pres->count_planes; i++) {
        plane = drmModeGetPlane(fd, pres->planes[i]);
        if (!plane)
            continue;
        type = drm_get_property_value(fd, plane->plane_id,
                                      DRM_MODE_OBJECT_PLANE, "type");
        if (type == 0 /* overlay */) {
            if (!overlay)
                overlay = drmModeGetPlane(fd, pres->planes[i]);
        }
        if (type == 1 /* primary */) {
            if (!primary)
                primary = drmModeGetPlane(fd, pres->planes[i]);
        }
        if (type == 2 /* cursor */) {
            if (!cursor)
                cursor = drmModeGetPlane(fd, pres->planes[i]);
        }
        drmModeFreePlane(plane);
    }
}

/* ------------------------------------------------------------------ */

bool drm_probe_format_fb(int fd, const struct fbformat *fmt)
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
                      int indent, bool libs, bool virtio)
{
    fprintf(fp, "%*s%-8s:  [%2d:0]  %-14s %-11s %c %c %c  %-16s",
            indent, "",
            fmt->name, fmt->bpp - 1, fmt->fields, fmt->bits,
            drm_probe_format_primary(fmt)  ? 'P' : '.',
            drm_probe_format_overlay(fmt)  ? 'O' : '.',
            drm_probe_format_cursor(fmt)   ? 'C' : '.',
            fmt->fourcc
            ? "fourcc  le"
            : "legacy  cpu " LE_BE("(le)", "(be)"));
    if (libs) {
        fprintf(fp, "  %-6s %-5s",
                fmt->pixman                          ? "pixman" : "",
                (fmt->cairo != CAIRO_FORMAT_INVALID) ? "cairo"  : "");
    }
    if (virtio) {
        if (fmt->virtio)
            fprintf(fp, "  %-6d", fmt->virtio);
    }
    fprintf(fp, "\n");
}

void drm_print_format_hdr(FILE *fp, int indent, bool libs, bool virtio)
{
    fprintf(fp, "%*s%-8s:  %-6s  %-14s %-11s %-6s %-16s",
            indent, "",
            "name", "bpp", "fields", "bits", "plane",
            "type    endian");
    if (libs) {
        fprintf(fp, "  lib support");
    }
    if (virtio) {
        fprintf(fp, "  virtio");
    }
    fprintf(fp, "\n");
}

/* ------------------------------------------------------------------ */

int drm_nr;
int drm_fd;
uint32_t fb_id;
drmModeConnector *drm_conn = NULL;
drmModeModeInfo *drm_mode = NULL;
drmModeEncoder *drm_enc = NULL;
drmVersion *version = NULL;

static drmModeCrtc *scrtc = NULL;

void drm_init_dev(int devnr, const char *output,
                  const char *modename, bool need_dumb,
                  int lease_fd)
{
    drmModeRes *res;
    char dev[64];
    char name[64];
    char m[64];
    int i, rc;
    uint64_t has_dumb;

    if (lease_fd >= 0) {
        drm_fd = lease_fd;
    } else {
        /* open device */
        snprintf(dev, sizeof(dev), DRM_DEV_NAME, DRM_DIR_NAME, devnr);
        drm_fd = device_open(dev);
    }
    drm_nr = devnr;
    version = drmGetVersion(drm_fd);

    if (need_dumb) {
        rc = drmGetCap(drm_fd, DRM_CAP_DUMB_BUFFER, &has_dumb);
        if (rc < 0 || !has_dumb) {
            fprintf(stderr, "no dumb buffer support\n");
            exit(1);
        }
    }
#if 0
    if (need_master) {
        rc = drmSetMaster(drm_fd);
        if (rc < 0) {
            fprintf(stderr, "drmSetMaster() failed: %s\n",
                    strerror(errno));
            exit(1);
        }
    }
#endif

    /* find connector */
    res = drmModeGetResources(drm_fd);
    if (res == NULL) {
        fprintf(stderr, "drmModeGetResources() failed\n");
        exit(1);
    }
    for (i = 0; i < res->count_connectors; i++) {
        drm_conn = drmModeGetConnector(drm_fd, res->connectors[i]);
        if (drm_conn &&
            (drm_conn->connection == DRM_MODE_CONNECTED) &&
            drm_conn->count_modes) {
            if (output) {
                drm_conn_name(drm_conn, name, sizeof(name));
                if (strcmp(name, output) == 0) {
                    break;
                }
            } else {
                break;
            }
        }
        drmModeFreeConnector(drm_conn);
        drm_conn = NULL;
    }
    if (!drm_conn) {
        if (output) {
            fprintf(stderr, "drm: output %s not found or disconnected\n",
                    output);
        } else {
            fprintf(stderr, "drm: no usable output found\n");
        }
        exit(1);
    }

    if (modename) {
        for (i = 0; i < drm_conn->count_modes; i++) {
            snprintf(m, sizeof(m), "%dx%d",
                     drm_conn->modes[i].hdisplay,
                     drm_conn->modes[i].vdisplay);
            if (strcmp(m, modename) == 0) {
                fprintf(stderr, "Using mode %s\n", modename);
                drm_mode = &drm_conn->modes[i];
                break;
            }
        }
    }
    if (!drm_mode) {
        drm_mode = &drm_conn->modes[0];
    }

    drm_enc = drmModeGetEncoder(drm_fd, drm_conn->encoder_id);
    if (drm_enc == NULL) {
        fprintf(stderr, "drmModeGetEncoder() failed\n");
        exit(1);
    }

    drm_plane_init(drm_fd);

    /* save crtc */
    scrtc = drmModeGetCrtc(drm_fd, drm_enc->crtc_id);
}

int drm_init_vgem(void)
{
    char dev[64];
    drmVersion *ver;
    int fd, i;

    for (i = 0; i < 128; i++) {
        if (i == drm_nr)
            continue;
#if 1
        snprintf(dev, sizeof(dev), "/dev/dri/card%d", i);
#else
        snprintf(dev, sizeof(dev), "/dev/dri/renderD%d", i + 128);
#endif
        fd = device_open(dev);
        if (fd < 0)
            goto out;
        ver = drmGetVersion(fd);
        fprintf(stderr, "%d: %s\n", i, ver->name);
        if (strcmp(ver->name, "vgem") == 0)
            return fd;
        close(fd);
    }

out:
    fprintf(stderr, "vgem not found, driver not loaded?\n");
    exit(1);
}

void drm_fini_dev(void)
{
    /* restore crtc */
    if (scrtc) {
        drmModeSetCrtc(drm_fd, scrtc->crtc_id, scrtc->buffer_id, scrtc->x, scrtc->y,
                       &drm_conn->connector_id, 1, &scrtc->mode);
    }
}

void drm_show_fb(void)
{
    int rc;

    rc = drmModeSetCrtc(drm_fd, drm_enc->crtc_id, fb_id, 0, 0,
                        &drm_conn->connector_id, 1,
                        drm_mode);
    if (rc < 0) {
        fprintf(stderr, "drmModeSetCrtc() failed: %s\n", strerror(errno));
        exit (1);
    }
}
