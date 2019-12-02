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
#include <sys/wait.h>
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

/* device caps */
bool have_export;
bool have_import;

/* dumb fb */
static struct drm_mode_create_dumb creq;
static const struct fbformat *fmt = NULL;
static uint8_t *fbmem;
static int dmabuf_fd;
static uint8_t *dmabuf_mem;

/* cairo + pixman */
static cairo_surface_t *cs;
static pixman_image_t *pxcs;
static pixman_image_t *pxfb;
static pixman_image_t *pxref;
static pixman_image_t *pxdma;

/* user options */
static cairo_surface_t *image;

/* ------------------------------------------------------------------ */

#define INDENT_WIDTH  4
#define NAME_WIDTH 16

static int test_passed = 0;
static int test_failed = 0;

static void print_head(const char *name)
{
    fprintf(stderr, "%*s%s\n",
            INDENT_WIDTH * 0, "",
            name);
}

static void print_test(const char *name, bool failed, const char *errmsg)
{
    fprintf(stderr, "%*s%-*s: %s",
            INDENT_WIDTH * 1, "",
            NAME_WIDTH, name,
            !failed ? "OK" : "FAILED");
    if (failed && errmsg)
        fprintf(stderr, " (%s)", errmsg);
    fprintf(stderr, "\n");

    if (failed)
        test_failed++;
    else
        test_passed++;
}

static void print_test_errno(const char *name, bool failed, int err)
{
    print_test(name, failed, strerror(err));
}

static void print_test_summary_and_exit()
{
    if (test_failed + test_passed) {
        fprintf(stderr, "%-*s: %d/%d passed\n",
                NAME_WIDTH + INDENT_WIDTH, "test summary",
                test_passed, test_passed + test_failed);
    }
    if (test_failed)
        exit(1);
    exit(0);
}

/* ------------------------------------------------------------------ */

static void drm_get_caps(void)
{
    uint64_t prime = 0;
    int rc;

    rc = drmGetCap(drm_fd, DRM_CAP_PRIME, &prime);
    if (rc < 0) {
        fprintf(stderr, "drmGetCap(DRM_CAP_PRIME): %s\n", strerror(errno));
        exit(1);
    }
    have_export = prime & DRM_PRIME_CAP_EXPORT;
    have_import = prime & DRM_PRIME_CAP_IMPORT;
}

static void drm_draw(bool autotest, int updatetest)
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
    if (updatetest) {
        snprintf(info2, sizeof(info2), "test #%d", updatetest);
        render_test(cr, drm_mode->hdisplay, drm_mode->vdisplay,
                    "display update", info2, NULL);
    } else if (image) {
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
    if (pxcs && pxref) {
        pixman_image_composite(PIXMAN_OP_SRC, pxcs, NULL, pxref,
                               0, 0,
                               0, 0,
                               0, 0,
                               drm_mode->hdisplay, drm_mode->vdisplay);
    }
}

static bool pixman_compare(pixman_image_t *p1,
                           pixman_image_t *p2)
{
    uint8_t *d1, *d2;
    int line, bpp, length;

    d1 = (void*)pixman_image_get_data(p1);
    d2 = (void*)pixman_image_get_data(p2);
    bpp = PIXMAN_FORMAT_BPP(pixman_image_get_format(p1));
    length = pixman_image_get_width(p1) * bpp / 8;
    for (line = 0; line < pixman_image_get_height(p1); line++) {
        if (memcmp(d1, d2, length) != 0) {
            fprintf(stderr, "mismatch line %d\n", line);
            return false;
        }
        d1 += pixman_image_get_stride(p1);
        d2 += pixman_image_get_stride(p2);
    }
    return true;
}

static void drm_check_content(const char *grp)
{
    bool fail;

    if (!pxref)
        return;

    print_head(grp);
    if (pxfb) {
        fail = pixman_compare(pxref, pxfb);
        print_test("check mmap", !fail, 0);
    }
    if (pxdma) {
        fail = pixman_compare(pxref, pxdma);
        print_test("check dma-buf", !fail, 0);
    }
}

static void drm_zap_mappings(void)
{
    madvise(fbmem, creq.size, MADV_DONTNEED);
    madvise(dmabuf_mem, creq.size, MADV_DONTNEED);
}

/* ------------------------------------------------------------------ */

static void drm_init_dumb_obj(int fd)
{
    struct drm_mode_map_dumb mreq;
    int rc;

    /* create gem object */
    memset(&creq, 0, sizeof(creq));
    creq.width = drm_mode->hdisplay;
    creq.height = drm_mode->vdisplay;
    creq.bpp = fmt->bpp;
    rc = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    if (rc < 0) {
        fprintf(stderr, "DRM_IOCTL_MODE_CREATE_DUMB: %s\n", strerror(errno));
        exit(1);
    }

    /* map gem object */
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
}

static void drm_init_dumb_fb(bool use_pixman, bool create_dmabuf)
{
    uint32_t zero = 0;
    int rc;

    drm_init_dumb_obj(drm_fd);

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

    if (create_dmabuf) {
        print_head("create dma-buf");
        rc = drmPrimeHandleToFD(drm_fd, creq.handle, 0, &dmabuf_fd);
        print_test_errno("dma-buf export", rc < 0, errno);
        if (rc == 0) {
            dmabuf_mem = mmap(NULL, creq.size, PROT_READ, MAP_SHARED, dmabuf_fd, 0);
            print_test_errno("dma-buf mmap", dmabuf_mem == MAP_FAILED, errno);
            if (dmabuf_mem != MAP_FAILED) {
                pxdma = pixman_image_create_bits(fmt->pixman,
                                                 creq.width,
                                                 creq.height,
                                                 (void*)dmabuf_mem,
                                                 creq.pitch);
            } else {
                dmabuf_mem = NULL;
            }
        }
    }

    if (use_pixman) {
        pxfb = pixman_image_create_bits(fmt->pixman,
                                        creq.width,
                                        creq.height,
                                        (void*)fbmem,
                                        creq.pitch);
        pxref = pixman_image_create_bits(fmt->pixman,
                                         creq.width,
                                         creq.height,
                                         NULL, 0);
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

static void drm_draw_dumb_fb(bool autotest, int updatetest)
{
    drm_draw(autotest, updatetest);
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
            "  -d           run dma-buf tests\n"
            "  -v           vgem dma-buf import test\n"
            "  -c <nr>      pick card\n"
            "  -o <name>    pick output\n"
            "  -s <secs>    set sleep time (default: 60)\n"
            "  -u <count>   try <count> display updates, with <secs> delay\n"
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
    int vgem_fd = -1;
    char *output = NULL;
    char *format = NULL;
    char *modename = NULL;
    bool dmabuf = false;
    bool autotest = false;
    bool pixman = false;
    bool vgem = false;
    int updatetest = 0;
    int c,i,pid,rc;

    for (;;) {
        c = getopt(argc, argv, "hpdavu:L:c:s:o:i:f:m:");
        if (c == -1)
            break;
        switch (c) {
        case 'd':
            dmabuf = true;
            pixman = true;
            break;
        case 'p':
            pixman = true;
            break;
        case 'a':
            autotest = true;
            break;
        case 'v':
            vgem = true;
            break;
        case 'u':
            updatetest = atoi(optarg);
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
    drm_get_caps();

    if (dmabuf && !have_export) {
        fprintf(stderr, "dambuf export not supported by %s\n", version->name);
        exit(1);
    }

    if (vgem && !have_import) {
        fprintf(stderr, "dambuf import not supported by %s\n", version->name);
        exit(1);
    }

    if (vgem) {
        vgem_fd = drm_init_vgem();
    }

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

    drm_init_dumb_fb(pixman, dmabuf);
    drm_draw_dumb_fb(autotest, 0);
    drm_check_content("pre-show content");
    drm_show_fb();
    drm_check_content("post-show content");
    drm_zap_mappings();
    drm_check_content("post-zap content");

    pid = fork();
    if (pid == 0) {
        test_passed = 0;
        test_failed = 0;
        drm_check_content("post-fork content");
        rc = (test_failed << 4) |
            (test_passed << 0);
        exit(rc);
    } else {
        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            rc = WEXITSTATUS(status);
            test_passed += (rc >> 0) & 0x0f;
            test_failed += (rc >> 4) & 0x0f;
        } else {
            test_failed++;
        }
    }

    if (updatetest) {
        for (i = 1; i <= updatetest; i++) {
            sleep(secs);
            drm_draw_dumb_fb(autotest, i);
            drm_check_content("post-update content");
        }
    }

    if (autotest)
        fprintf(stdout, "---ok---\n");
    tty_raw();
    kbd_wait(secs);
    kbd_read();
    tty_restore();

    drm_fini_dev();
    logind_fini();

    print_test_summary_and_exit();
}
