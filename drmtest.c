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
#include <sys/mman.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <gbm.h>
#include <epoxy/gl.h>
#include <epoxy/egl.h>

#include <cairo.h>
#include <cairo-gl.h>

#include "drmtools.h"
#include "render.h"

/* ------------------------------------------------------------------ */

/* device */
static int fd;
static drmModeConnector *conn = NULL;
static drmModeEncoder *enc = NULL;
static drmModeModeInfo *mode = NULL;
static drmModeCrtc *scrtc = NULL;
static uint32_t fb_id;

/* dumb fb */
static struct drm_mode_create_dumb creq;
static uint8_t *fbmem;

/* opengl fb */
static struct gbm_device *gbm_dev;
static struct gbm_surface *gbm_surface;
static EGLDisplay dpy;
static EGLConfig cfg;
static EGLContext ctx;
static EGLSurface surface;

/* cairo */
cairo_surface_t *cs;
cairo_t *cc;

/* ------------------------------------------------------------------ */

static void drm_init_dev(int devnr, const char *output,
                         bool need_dumb, bool need_master)
{
    drmModeRes *res;
    char dev[64];
    char name[64];
    int i, rc;
    uint64_t has_dumb;

    /* open device */
    snprintf(dev, sizeof(dev), DRM_DEV_NAME, DRM_DIR_NAME, devnr);
    fd = open(dev, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", dev, strerror(errno));
        exit(1);
    }

    if (need_dumb) {
        rc = drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb);
        if (rc < 0 || !has_dumb) {
            fprintf(stderr, "no dumb buffer support\n");
            exit(1);
        }
    }
    if (need_master) {
        rc = drmSetMaster(fd);
        if (rc < 0) {
            fprintf(stderr, "drmSetMaster() failed: %s (X11 running?)\n",
                    strerror(errno));
            exit(1);
        }
    }

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
    mode = &conn->modes[0];
    enc = drmModeGetEncoder(fd, conn->encoder_id);
    if (enc == NULL) {
        fprintf(stderr, "drmModeGetEncoder() failed\n");
        exit(1);
    }

    /* save crtc */
    scrtc = drmModeGetCrtc(fd, enc->crtc_id);
}

static void drm_fini_dev(void)
{
    /* restore crtc */
    if (scrtc) {
        drmModeSetCrtc(fd, scrtc->crtc_id, scrtc->buffer_id, scrtc->x, scrtc->y,
                       &conn->connector_id, 1, &scrtc->mode);
    }
}

static void drm_show_fb(void)
{
    int rc;

    rc = drmModeSetCrtc(fd, enc->crtc_id, fb_id, 0, 0,
                        &conn->connector_id, 1,
                        &conn->modes[0]);
    if (rc < 0) {
        fprintf(stderr, "drmModeSetCrtc() failed\n");
        exit (1);
    }
}

/* ------------------------------------------------------------------ */

static void drm_init_dumb_fb(void)
{
    struct drm_mode_map_dumb mreq;
    int rc;

    /* create framebuffer */
    memset(&creq, 0, sizeof(creq));
    creq.width = mode->hdisplay;
    creq.height = mode->vdisplay;
    creq.bpp = 32;
    rc = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    if (rc < 0) {
        fprintf(stderr, "DRM_IOCTL_MODE_CREATE_DUMB: %s\n", strerror(errno));
        exit(1);
    }
    rc = drmModeAddFB(fd, creq.width, creq.height, 24, 32, creq.pitch,
                      creq.handle, &fb_id);
    if (rc < 0) {
        fprintf(stderr, "drmModeAddFB() failed\n");
        exit(1);
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
                                             CAIRO_FORMAT_ARGB32,
                                             creq.width,
                                             creq.height,
                                             creq.pitch);
}

static void drm_draw_dumb_fb(void)
{
    char name[64];
    char info[80];
    cairo_t *cr;

    drm_conn_name(conn, name, sizeof(name));
    snprintf(info, sizeof(info), "%s: %dx%d",
             name, mode->hdisplay, mode->vdisplay);
    cr = cairo_create(cs);
    render_test(cr, mode->hdisplay, mode->vdisplay, info);
    cairo_destroy(cr);
    drmModeDirtyFB(fd, fb_id, 0, 0);
}

/* ------------------------------------------------------------------ */

static void drm_init_egl(void)
{
    static const EGLint conf_att[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE,   5,
        EGL_GREEN_SIZE, 5,
        EGL_BLUE_SIZE,  5,
        EGL_ALPHA_SIZE, 0,
        EGL_NONE,
    };
    static const EGLint ctx_att[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    EGLint major, minor;
    EGLBoolean b;
    EGLint n;

    gbm_dev = gbm_create_device(fd);
    if (!gbm_dev) {
        fprintf(stderr, "egl: gbm_create_device failed\n");
        exit(1);
    }

    gbm_surface = gbm_surface_create(gbm_dev,
                                     mode->hdisplay, mode->vdisplay,
                                     GBM_FORMAT_XRGB8888,
                                     GBM_BO_USE_RENDERING);
    if (!gbm_surface) {
        fprintf(stderr, "egl: gbm_create_surface failed\n");
        exit(1);
    }

    dpy = eglGetDisplay(gbm_dev);
    if (dpy == EGL_NO_DISPLAY) {
        fprintf(stderr, "egl: eglGetDisplay failed\n");
        exit(1);
    }

    b = eglInitialize(dpy, &major, &minor);
    if (b == EGL_FALSE) {
        fprintf(stderr, "egl: eglInitialize failed\n");
        exit(1);
    }

    b = eglBindAPI(EGL_OPENGL_ES_API);
    if (b == EGL_FALSE) {
        fprintf(stderr, "egl: eglBindAPI failed\n");
        exit(1);
    }

    b = eglChooseConfig(dpy, conf_att, &cfg, 1, &n);
    if (b == EGL_FALSE || n != 1) {
        fprintf(stderr, "egl: eglChooseConfig failed\n");
        exit(1);
    }

    ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_att);
    if (ctx == EGL_NO_CONTEXT) {
        fprintf(stderr, "egl: eglCreateContext failed\n");
        exit(1);
    }

    b = eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx);
    if (b == EGL_FALSE) {
        fprintf(stderr, "egl: eglMakeCurrent(EGL_NO_SURFACE) failed\n");
        exit(1);
    }

    surface = eglCreateWindowSurface (dpy, cfg,
                                      (EGLNativeWindowType)gbm_surface,
                                      NULL);
    if (!surface) {
        fprintf(stderr, "egl: eglCreateWindowSurface failed\n");
        exit(1);
    }

    b = eglMakeCurrent(dpy, surface, surface, ctx);
    if (b == EGL_FALSE) {
        fprintf(stderr, "egl: eglMakeCurrent(surface) failed\n");
        exit(1);
    }
}

static void drm_draw_egl(void)
{
    glViewport(0, 0, mode->hdisplay, mode->vdisplay);
    glClearColor(0.5, 0, 0, 0); /* red */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static void drm_make_egl_fb(void)
{
    struct gbm_bo *bo;
    EGLBoolean b;
    uint32_t handle, stride;

    b = eglSwapBuffers(dpy, surface);
    if (!b) {
        fprintf(stderr, "egl: eglSwapBuffers failed");
        exit(1);
    }

    bo = gbm_surface_lock_front_buffer(gbm_surface);
    if (!bo) {
        fprintf(stderr, "egl: gbm_surface_lock_front_buffer failed");
        exit(1);
    }
    handle = gbm_bo_get_handle(bo).u32;
    stride = gbm_bo_get_stride(bo);

    drmModeAddFB(fd, mode->hdisplay, mode->vdisplay, 24, 32,
                 stride, handle, &fb_id);
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
            "  -g         openngl mode\n"
#if 0
            "  -d         debug mode (opengl)\n"
#endif
            "\n");
}

int main(int argc, char **argv)
{
    int card = 0;
    int secs = 10;
    bool gl = false;
    char *output = NULL;
    int c;

    for (;;) {
        c = getopt(argc, argv, "hgdc:s:o:");
        if (c == -1)
            break;
        switch (c) {
        case 'c':
            card = atoi(optarg);
            break;
        case 's':
            secs = atoi(optarg);
            break;
        case 'o':
            output = optarg;
            break;
        case 'g':
            gl = true;
            break;
#if 0
        case 'd':
            setenv("EGL_LOG_LEVEL", "debug", true);
            setenv("LIBGL_DEBUG", "verbose", true);
            break;
#endif
        case 'h':
            usage(stdout);
            exit(0);
        default:
            usage(stderr);
            exit(1);
        }
    }

    if (gl) {
        drm_init_dev(card, output, true, true);
        drm_init_egl();
        drm_draw_egl();
        drm_make_egl_fb();
    } else {
        drm_init_dev(card, output, false, true);
        drm_init_dumb_fb();
        drm_draw_dumb_fb();
    }
    drm_show_fb();
    sleep(secs);
    drm_fini_dev();
    return 0;
}
