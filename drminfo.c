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
#include <libdrm/drm_fourcc.h>

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

static const char* drm_info_modifier_string(uint64_t mod){
    switch (mod) {
#define CASE_(X) case X: return #X
        CASE_(DRM_FORMAT_MOD_INVALID);
        CASE_(DRM_FORMAT_MOD_LINEAR);

        /* Intel */
        CASE_(I915_FORMAT_MOD_X_TILED);
        CASE_(I915_FORMAT_MOD_Y_TILED);
        CASE_(I915_FORMAT_MOD_Yf_TILED);
        CASE_(I915_FORMAT_MOD_Y_TILED_CCS);
        CASE_(I915_FORMAT_MOD_Yf_TILED_CCS);

        /* Samsung */
        CASE_(DRM_FORMAT_MOD_SAMSUNG_64_32_TILE);

        /* Vivante */
        CASE_(DRM_FORMAT_MOD_VIVANTE_TILED);
        CASE_(DRM_FORMAT_MOD_VIVANTE_SUPER_TILED);
        CASE_(DRM_FORMAT_MOD_VIVANTE_SPLIT_TILED);
        CASE_(DRM_FORMAT_MOD_VIVANTE_SPLIT_SUPER_TILED);

        /* NVIDIA */
        CASE_(DRM_FORMAT_MOD_NVIDIA_TEGRA_TILED);
        CASE_(DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_ONE_GOB);
        CASE_(DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_TWO_GOB);
        CASE_(DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_FOUR_GOB);
        CASE_(DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_EIGHT_GOB);
        CASE_(DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_SIXTEEN_GOB);
        CASE_(DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_THIRTYTWO_GOB);

        /* Broadcom */
        CASE_(DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED);

#undef CASE_
        default:
        {
            static char mod_string[32] = ""; /* not thread-safe! */
            snprintf(mod_string, sizeof(mod_string), "%#lx", mod);
            return mod_string;
        }
    }
}

static size_t drm_modifiers_for_format(int fd, uint32_t plane_id,
                                       uint32_t drm_format,
                                       uint64_t **drm_modifiers)
{
   /* Get the properties of the plane */
   drmModeObjectProperties *props =
       drmModeObjectGetProperties(fd, plane_id, DRM_MODE_OBJECT_PLANE);
   if (!props)
       return 0;

   /* Find the blob the contains the formats and their modifiers */
   uint32_t blob_id = 0;
   for (size_t i = 0; i< props->count_props; i++) {
      const drmModePropertyPtr prop =
          drmModeGetProperty(fd, props->props[i]);

      if (!strcmp(prop->name, "IN_FORMATS")) {
          blob_id = props->prop_values[i];
          drmModeFreeProperty(prop);
          break;
      }

      drmModeFreeProperty(prop);
   }

   /* Property not found, which means old kernel, so definitely no
    * modifiers support */
   if (blob_id == 0) {
       drmModeFreeObjectProperties(props);
       return 0;
   }

   /* Grab the IN_FORMATS blob */
   drmModePropertyBlobRes *blob = drmModeGetPropertyBlob(fd, blob_id);
   if (!blob) {
       drmModeFreeObjectProperties(props);
       return 0;
   }

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
   drmModeFreeObjectProperties(props);

   *drm_modifiers = modifiers;
   return modifiers_count;
}

static void drm_info_plane(int fd, drmModePlane *plane)
{
    int i;

    fprintf(stdout, "plane: %d, crtc: %d, fb: %d\n",
            plane->plane_id, plane->crtc_id, plane->fb_id);

    fprintf(stdout, "    formats:  (modifier(s) supported by this format)\n");
    for (i = 0; i < plane->count_formats; i++) {
        size_t m;
        uint64_t *drm_modifiers;
        size_t drm_modifiers_count =
          drm_modifiers_for_format(fd, plane->plane_id,
                                   plane->formats[i],
                                   &drm_modifiers);

        fprintf(stdout, "      - %c%c%c%c",
                (plane->formats[i] >>  0) & 0xff,
                (plane->formats[i] >>  8) & 0xff,
                (plane->formats[i] >> 16) & 0xff,
                (plane->formats[i] >> 24) & 0xff);

        if (drm_modifiers_count == 0) {
            fprintf(stdout, "\n");
            continue;
        }

        fprintf(stdout, "  (");
        for (m = 0; m < drm_modifiers_count; m++)
            fprintf(stdout, "%s%s",
                    (m == 0) ? "" : ", ",
                    drm_info_modifier_string(drm_modifiers[m]));
        fprintf(stdout, ")\n");

        free(drm_modifiers);
    }
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
    drm_print_format_hdr(stdout, 4, true);
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

static void drm_info_misc(int fd)
{
    char *busid;

    busid = drmGetBusid(fd);
    if (busid) {
        fprintf(stdout, "busid: \"%s\"\n", busid);
    }
    fprintf(stdout, "\n");
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
            "  -a         print all card info\n"
            "  -m         print misc card info (busid)\n"
            "  -o         print supported outputs (crtcs)\n"
            "  -p         print supported planes\n"
            "  -f         print supported formats\n"
            "  -l         list all known formats\n"
            "\n");
}

int main(int argc, char **argv)
{
    int card = 0;
    int c, fd;
    bool misc = false;
    bool conn = false;
    bool plane = false;
    bool format = false;

    for (;;) {
        c = getopt(argc, argv, "hlamopfc:");
        if (c == -1)
            break;
        switch (c) {
        case 'c':
            card = atoi(optarg);
            break;
        case 'l':
            list_formats(stdout);
            exit(0);
        case 'a':
            misc = true;
            conn = true;
            plane = true;
            format = true;
            break;
        case 'm':
            misc = true;
            break;
        case 'o':
            conn = true;
            break;
        case 'p':
            plane = true;
            break;
        case 'f':
            format = true;
            break;
        case 'h':
            usage(stdout);
            exit(0);
        default:
            usage(stderr);
            exit(1);
        }
    }

    fd = drm_open(card);
    if (misc)
        drm_info_misc(fd);
    if (conn)
        drm_info_conns(fd);
    if (plane)
        drm_info_planes(fd);
    if (format)
        drm_info_fmts(fd);
    return 0;
}
