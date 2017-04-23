#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <time.h>
#include <termios.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <drm/drm_fourcc.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <gbm.h>
#include <epoxy/gl.h>
#include <epoxy/egl.h>

#include <cairo.h>
#include <cairo-gl.h>

#include "drmtools.h"
#include "render.h"
#include "image.h"

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
static const struct fbformat *fmt = NULL;
static uint8_t *fbmem;

/* opengl fb */
static struct gbm_device *gbm_dev;
static struct gbm_surface *gbm_surface;
static EGLDisplay dpy;
static EGLConfig cfg;
static EGLContext ctx;
static EGLSurface surface;

/* cairo */
cairo_device_t *cd;
cairo_surface_t *cs;
cairo_t *cc;

/* user options */
cairo_surface_t *image;

/* ------------------------------------------------------------------ */

static void drm_init_dev(int devnr, const char *output, bool need_dumb)
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

static void drm_draw(const char *text)
{
    char name[64];
    char info[80];
    cairo_t *cr;

    drm_conn_name(conn, name, sizeof(name));
    snprintf(info, sizeof(info), "drmtest: %dx%d at %s",
             mode->hdisplay, mode->vdisplay, name);
    cr = cairo_create(cs);
    if (image) {
        render_image(cr, mode->hdisplay, mode->vdisplay, image);
    } else {
        render_test(cr, mode->hdisplay, mode->vdisplay, info, text);
    }
    cairo_destroy(cr);
}

/* ------------------------------------------------------------------ */

static void drm_init_dumb_fb(void)
{
    struct drm_mode_map_dumb mreq;
    uint32_t zero = 0;
    int rc;

    /* create framebuffer */
    memset(&creq, 0, sizeof(creq));
    creq.width = mode->hdisplay;
    creq.height = mode->vdisplay;
    creq.bpp = fmt->bpp;
    rc = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    if (rc < 0) {
        fprintf(stderr, "DRM_IOCTL_MODE_CREATE_DUMB: %s\n", strerror(errno));
        exit(1);
    }

    if (fmt->fourcc) {
        rc = drmModeAddFB2(fd, creq.width, creq.height, fmt->fourcc,
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
        rc = drmModeAddFB(fd, creq.width, creq.height, fmt->depth, fmt->bpp,
                          creq.pitch, creq.handle, &fb_id);
        if (rc < 0) {
            fprintf(stderr, "drmModeAddFB() failed (bpp %d, depth %d)\n",
                    fmt->bpp, fmt->depth);
            exit(1);
        }
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
                                             fmt->cairo,
                                             creq.width,
                                             creq.height,
                                             creq.pitch);
}

static void drm_draw_dumb_fb(void)
{
    char text[80];

    if (fmt->fourcc) {
        snprintf(text, sizeof(text),
                 "dumb framebuffer, bpp %d, fourcc %c%c%c%c (ADDFB2)",
                 fmt->bpp,
                 (fmt->fourcc >>  0) & 0xff,
                 (fmt->fourcc >>  8) & 0xff,
                 (fmt->fourcc >> 16) & 0xff,
                 (fmt->fourcc >> 24) & 0xff);
    } else {
        snprintf(text, sizeof(text),
                 "dumb framebuffer, bpp %d, depth %d (legacy ADDFB)",
                 fmt->bpp, fmt->depth);
    }
    drm_draw(text);
    drmModeDirtyFB(fd, fb_id, 0, 0);
}

/* ------------------------------------------------------------------ */

static void drm_init_egl(void)
{
    static const EGLint conf_att[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE,   5,
        EGL_GREEN_SIZE, 5,
        EGL_BLUE_SIZE,  5,
        EGL_ALPHA_SIZE, 0,
        EGL_NONE,
    };
    static const EGLint ctx_att[] = {
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

    b = eglBindAPI(EGL_OPENGL_API);
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

    cd = cairo_egl_device_create(dpy, ctx);
    cs = cairo_gl_surface_create_for_egl(cd, surface,
                                         mode->hdisplay,
                                         mode->vdisplay);
}

static void drm_draw_egl(void)
{
    char text[80];

    snprintf(text, sizeof(text),
             "egl: %s", glGetString(GL_RENDERER));
    drm_draw(text);
    cairo_gl_surface_swapbuffers(cs);
}

static void drm_make_egl_fb(void)
{
    struct gbm_bo *bo;
    uint32_t handle, stride;

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

struct termios  saved_attributes;
int             saved_fl;

void tty_raw(void)
{
    struct termios tattr;

    fcntl(STDIN_FILENO, F_GETFL, &saved_fl);
    tcgetattr (0, &saved_attributes);

    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    memcpy(&tattr,&saved_attributes,sizeof(struct termios));
    tattr.c_lflag &= ~(ICANON|ECHO);
    tattr.c_cc[VMIN] = 1;
    tattr.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);
}

void tty_restore(void)
{
    fcntl(STDIN_FILENO, F_SETFL, saved_fl);
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_attributes);
}

int kbd_wait(int timeout)
{
    struct timeval limit;
    fd_set set;
    int rc;

    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    limit.tv_sec = timeout;
    limit.tv_usec = 0;
    rc = select(STDIN_FILENO + 1, &set, NULL, NULL,
                timeout ? &limit : NULL);
    return rc;
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
            "  -i <file>  load and display image <file>\n"
            "  -f <fmt>   pick framebuffer format\n"
            "  -g         openngl mode\n"
#if 0
            "  -d         debug mode (opengl)\n"
#endif
            "\n");
}

int main(int argc, char **argv)
{
    int card = 0;
    int secs = 60;
    bool gl = false;
    char *output = NULL;
    char *format = NULL;
    char buf[32];
    int c,i;

    for (;;) {
        c = getopt(argc, argv, "hgdc:s:o:i:f:");
        if (c == -1)
            break;
        switch (c) {
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

    if (format) {
        for (i = 0; i < fmtcnt; i++) {
            if (strcmp(format, fmts[i].name) == 0) {
                fmt = &fmts[i];
            }
        }
        if (!fmt) {
            fprintf(stderr, "unknown format %s, valid choices are:\n", format);
            for (i = 0; i < fmtcnt; i++) {
                if (fmts[i].cairo == CAIRO_FORMAT_INVALID)
                    continue;
                drm_print_format(stderr, &fmts[i]);
            }
            exit(1);
        }
    } else {
        fmt = &fmts[0];
    }

    if (gl) {
        drm_init_dev(card, output, true);
        drm_init_egl();
        drm_draw_egl();
        drm_make_egl_fb();
    } else {
        drm_init_dev(card, output, false);
        drm_init_dumb_fb();
        drm_draw_dumb_fb();
    }
    drm_show_fb();

    tty_raw();
    kbd_wait(secs);
    read(0, buf, sizeof(buf));
    tty_restore();

    drm_fini_dev();
    return 0;
}
