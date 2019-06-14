#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <termios.h>

#include <sys/ioctl.h>
#include <libdrm/drm_fourcc.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cairo.h>
#include <pixman.h>

#include "drmtools.h"
#include "drm-lease.h"
#include "logind.h"

static int ttycols = 80;

/* ------------------------------------------------------------------ */

static void drm_list_properties(int fd, uint32_t id, uint32_t objtype)
{
   drmModeObjectProperties *props =
       drmModeObjectGetProperties(fd, id, objtype);
   drmModePropertyPtr prop;
   uint32_t i;

   for (i = 0; i < props->count_props; i++) {
       prop = drmModeGetProperty(fd, props->props[i]);
       if (prop->count_values) {
           fprintf(stdout, "    property: %s, value %" PRId64 "\n",
                   prop->name, props->prop_values[i]);
       } else {
           fprintf(stdout, "    property: %s\n", prop->name);
       }
       drmModeFreeProperty(prop);
   }
   drmModeFreeObjectProperties(props);
}

static drmModePropertyBlobRes *
drm_get_property_blob(int fd, uint32_t id, uint32_t objtype, const char *name)
{
    uint64_t blob_id =
        drm_get_property_value(fd, id, objtype, name);

   if (!blob_id)
       return NULL;
   return drmModeGetPropertyBlob(fd, blob_id);
}

static void drm_info_conn(int fd, drmModeConnector *conn,
                          bool print_properties)
{
    drmModeEncoder *enc;
    drmModeCrtc *crtc;
    char name[64];
    int e, c, m;

    drm_conn_name(conn, name, sizeof(name));
    fprintf(stdout, "%s (#%d), %s\n",
            name, conn->connector_id,
            drm_connector_mode_name(conn->connection));

    for (e = 0; e < conn->count_encoders; e++) {
        enc = drmModeGetEncoder(fd, conn->encoders[e]);
        if (!enc)
            continue;
        fprintf(stdout, "    encoder: %s (#%d)",
                drm_encoder_type_name(enc->encoder_type),
                enc->encoder_id);
        if (enc->encoder_id == conn->encoder_id)
            fprintf(stdout, ", active");
        if (enc->crtc_id) {
            crtc = drmModeGetCrtc(fd, enc->crtc_id);
            if (crtc) {
                fprintf(stdout, ", crtc #%d", crtc->crtc_id);
                fprintf(stdout, ", fb #%d", crtc->buffer_id);
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

    if (print_properties)
        drm_list_properties(fd, conn->connector_id,
                            DRM_MODE_OBJECT_CONNECTOR);

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

static void drm_info_conns(int fd, bool print_properties)
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

        drm_info_conn(fd, conn, print_properties);
        drmModeFreeConnector(conn);
        fprintf(stdout, "\n");
    }
}

static const struct {
    uint64_t mod;
    const char *name;
} drm_fmt_mod_name[] = {
#include "drmfmtmods.h"
};

static const char *drm_info_modifier_string(uint64_t mod)
{
    int i = 0;

    while (i < (sizeof(drm_fmt_mod_name)/sizeof(drm_fmt_mod_name[0]))) {
        if (drm_fmt_mod_name[i].mod == mod)
            return drm_fmt_mod_name[i].name;
        i++;
    }
    return "unknown";
}

static size_t drm_modifiers_for_format(int fd, uint32_t plane_id,
                                       uint32_t drm_format,
                                       uint64_t **drm_modifiers)
{
   /* Grab the IN_FORMATS blob */
    drmModePropertyBlobRes *blob =
        drm_get_property_blob(fd, plane_id, DRM_MODE_OBJECT_PLANE,
                              "IN_FORMATS");

   if (!blob)
       return 0;

   /* Get the formats and modifiers out of the blob */
   struct drm_format_modifier_blob *fmt_mod_blob = blob->data;
   uint32_t *blob_formats = (uint32_t*)((char*)fmt_mod_blob +
                                        fmt_mod_blob->formats_offset);
   struct drm_format_modifier *blob_modifiers =
       (struct drm_format_modifier *)((char*)fmt_mod_blob +
                                     fmt_mod_blob->modifiers_offset);

   /* Find the format we care about in the list */
   size_t format_index = 0;
   for (size_t i = 0; i < fmt_mod_blob->count_formats; i++) {
       if (blob_formats[i] == drm_format) {
           format_index = i;
           break;
      }
   }

   /* Get the list of modifiers supported by that format */
   uint32_t modifiers_count = 0;
   uint64_t *modifiers = NULL;
   for (size_t i = 0; i < fmt_mod_blob->count_modifiers; i++) {
       struct drm_format_modifier *mod = &blob_modifiers[i];

       if ((format_index < mod->offset) || (format_index > mod->offset + 63))
           continue;
       if (!(mod->formats & (1 << (format_index - mod->offset))))
           continue;

       modifiers = realloc(modifiers,
                           (modifiers_count + 1) *
                           sizeof(modifiers[0]));
       modifiers[modifiers_count++] = mod->modifier;
   }

   drmModeFreePropertyBlob(blob);

   *drm_modifiers = modifiers;
   return modifiers_count;
}

static const char *drm_info_plane_type_string(uint64_t type)
{
    switch (type) {
    case 0:
        return "overlay";
    case 1:
        return "primary";
    case 2:
        return "cursor";
    default:
        return "unknown";
    }
}

static void drm_info_plane(int fd, drmModePlane *plane,
                           bool print_modifiers, bool print_properties)
{
    uint64_t type = drm_get_property_value(fd, plane->plane_id,
                                           DRM_MODE_OBJECT_PLANE, "type");
    int i;

    fprintf(stdout, "plane: %d, crtc: %d, fb: %d, type: %s\n",
            plane->plane_id, plane->crtc_id, plane->fb_id,
            drm_info_plane_type_string(type));
    if (print_properties)
        drm_list_properties(fd, plane->plane_id, DRM_MODE_OBJECT_PLANE);

    if (!print_modifiers) {
        fprintf(stdout, "    formats:");
        for (i = 0; i < plane->count_formats; i++)
            fprintf(stdout, " %c%c%c%c",
                    (plane->formats[i] >>  0) & 0xff,
                    (plane->formats[i] >>  8) & 0xff,
                    (plane->formats[i] >> 16) & 0xff,
                    (plane->formats[i] >> 24) & 0xff);
        fprintf(stdout, "\n");
        return;
    }

    fprintf(stdout, "    format   modifiers\n");
    for (i = 0; i < plane->count_formats; i++) {
        if (print_modifiers) {
            uint64_t *drm_modifiers = NULL;
            size_t drm_modifiers_count;
            size_t m;
            int pos = 12;
            drm_modifiers_count =
                drm_modifiers_for_format(fd, plane->plane_id,
                                         plane->formats[i],
                                         &drm_modifiers);
            fprintf(stdout, "    %c%c%c%c    ",
                    (plane->formats[i] >>  0) & 0xff,
                    (plane->formats[i] >>  8) & 0xff,
                    (plane->formats[i] >> 16) & 0xff,
                    (plane->formats[i] >> 24) & 0xff);
            for (m = 0; m < drm_modifiers_count; m++) {
                const char *name = drm_info_modifier_string(drm_modifiers[m]);
                int len = strlen(name);
                if (pos + len + 2 > ttycols) {
                    fprintf(stdout, "\n            ");
                    pos = 12;
                }
                fprintf(stdout, " %s", name);
                pos += len + 1;
            }
            free(drm_modifiers);
            fprintf(stdout, "\n");
        }
    }
}

static void drm_info_planes(int fd, bool print_modifiers, bool print_properties)
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

        drm_info_plane(fd, plane, print_modifiers, print_properties);
        drmModeFreePlane(plane);
        fprintf(stdout, "\n");
    }
}

static void drm_info_fmts(int fd, bool listonly)
{
    bool first = true;
    int i;

    drm_plane_init(fd);
    if (listonly) {
        for (i = 0; i < fmtcnt; i++) {
            if (!drm_probe_format_fb(fd, &fmts[i]))
                continue;
            if (!drm_probe_format_primary(&fmts[i]))
                continue;
            if (!fmts[i].pixman)
                continue;
            fprintf(stdout, "%s%s", first ? "" : " ", fmts[i].name);
            first = false;
        }
        fprintf(stdout, "\n");
    } else {
        fprintf(stdout, "framebuffer formats\n");
        drm_print_format_hdr(stdout, 4, true, false);
        for (i = 0; i < fmtcnt; i++) {
            if (!drm_probe_format_fb(fd, &fmts[i]))
                continue;
            drm_print_format(stdout, &fmts[i], 4, true, false);
        }
        fprintf(stdout, "\n");
    }
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

static void drm_info_misc(int fd)
{
    char *busid;

    version = drmGetVersion(fd);
    fprintf(stdout, "name    : \"%s\"\n", version->name);
    fprintf(stdout, "desc    : \"%s\"\n", version->desc);
    fprintf(stdout, "date    : \"%s\"\n", version->date);
    fprintf(stdout, "version : v%d.%d.%d\n",
            version->version_major, version->version_minor,
            version->version_patchlevel);

    busid = drmGetBusid(fd);
    if (busid) {
        fprintf(stdout, "busid   : \"%s\"\n", busid);
    }

    fprintf(stdout, "\n");
}

static void drm_info_caps(int fd)
{
    static const char *caps[] = {
        [ DRM_CAP_DUMB_BUFFER          ] = "DUMB_BUFFER",
        [ DRM_CAP_VBLANK_HIGH_CRTC     ] = "VBLANK_HIGH_CRTC",
        [ DRM_CAP_DUMB_PREFERRED_DEPTH ] = "DUMB_PREFERRED_DEPTH",
        [ DRM_CAP_DUMB_PREFER_SHADOW   ] = "DUMB_PREFER_SHADOW",
        [ DRM_CAP_PRIME                ] = "PRIME",
        [ DRM_CAP_TIMESTAMP_MONOTONIC  ] = "TIMESTAMP_MONOTONIC",
        [ DRM_CAP_ASYNC_PAGE_FLIP      ] = "ASYNC_PAGE_FLIP",
        [ DRM_CAP_CURSOR_WIDTH         ] = "CURSOR_WIDTH",
        [ DRM_CAP_CURSOR_HEIGHT        ] = "CURSOR_HEIGHT",
        [ DRM_CAP_ADDFB2_MODIFIERS     ] = "ADDFB2_MODIFIERS",
        [ DRM_CAP_PAGE_FLIP_TARGET     ] = "PAGE_FLIP_TARGET",
        [ DRM_CAP_CRTC_IN_VBLANK_EVENT ] = "CRTC_IN_VBLANK_EVENT",
        [ DRM_CAP_SYNCOBJ              ] = "SYNCOBJ",
    };
    uint64_t value;
    int i, rc;

    fprintf(stdout, "capabilities\n");
    for (i = 0; i < sizeof(caps)/sizeof(caps[0]); i++) {
        if (!caps[i])
            continue;
        value = 0;
        rc = drmGetCap(fd, i, &value);
        if (rc < 0)
            continue;
        fprintf(stdout, "    %-22s: %3" PRId64, caps[i], value);
        switch (i) {
        case DRM_CAP_PRIME:
            if (value) {
                bool im = value & DRM_PRIME_CAP_IMPORT;
                bool ex = value & DRM_PRIME_CAP_EXPORT;
                fprintf(stdout, "  (%s%s%s)",
                        im       ? "import" : "",
                        im && ex ? " + "    : "",
                        ex       ? "export" : "");
            }
            break;
        }
        fprintf(stdout, "\n");
    }
    fprintf(stdout, "\n");
}

static void list_formats(FILE *fp)
{
    int i;

    fprintf(stdout, "all known framebuffer formats (rgb + packed yuv)\n");
    drm_print_format_hdr(stdout, 0, true, false);
    for (i = 0; i < fmtcnt; i++) {
        drm_print_format(stdout, &fmts[i], 0, true, false);
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
            "  -h           print this text\n"
            "  -c <nr>      pick card\n"
            "  -a           print all card info\n"
            "  -A           print all card info, with plane modifiers\n"
            "  -m           print misc card info\n"
            "  -s           print capabilities\n"
            "  -o           print supported outputs (crtcs)\n"
            "  -p           print supported planes\n"
            "  -P           print supported planes, with modifiers\n"
            "  -f           print supported formats\n"
            "  -F           print testable (drmtest) formats\n"
            "  -r           list properties\n"
            "  -l           list all known formats\n"
            "  -L <output>  get a drm lease for output\n"
            "\n");
}

int main(int argc, char **argv)
{
    int card = 0;
    int lease_fd = -1;
    int c, fd;
    bool misc = false;
    bool caps = false;
    bool conn = false;
    bool plane = false;
    bool modifiers = false;
    bool properties = false;
    bool format = false;
    bool listonly = false;
    char *columns;

    for (;;) {
        c = getopt(argc, argv, "hlaAmsopPfFrL:c:");
        if (c == -1)
            break;
        switch (c) {
        case 'c':
            card = atoi(optarg);
            break;
        case 'l':
            list_formats(stdout);
            exit(0);
        case 'A':
            modifiers = true;
            /* fall through */
        case 'a':
            misc = true;
            caps = true;
            conn = true;
            plane = true;
            format = true;
            break;
        case 'm':
            misc = true;
            break;
        case 's':
            caps = true;
            break;
        case 'o':
            conn = true;
            break;
        case 'P':
            modifiers = true;
            /* fall through */
        case 'p':
            plane = true;
            break;
        case 'r':
            properties = true;
            break;
        case 'F':
            /* fall through */
            listonly = true;
        case 'f':
            format = true;
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

    columns = getenv("COLUMNS");
    if (columns) {
        ttycols = atoi(columns);
    } else {
        struct winsize win = { 0 };
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) == 0 &&
            win.ws_col != 0) {
            ttycols = win.ws_col;
        }
    }

    logind_init();

    if (lease_fd >= 0) {
        fd = lease_fd;
    } else {
        fd = drm_open(card);
    }

    if (misc)
        drm_info_misc(fd);
    if (caps)
        drm_info_caps(fd);
    if (conn)
        drm_info_conns(fd, properties);
    if (plane)
        drm_info_planes(fd, modifiers, properties);
    if (format)
        drm_info_fmts(fd, listonly);

    logind_fini();

    return 0;
}
