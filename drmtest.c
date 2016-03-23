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
static struct gbm_device *gbm;
static EGLDisplay dpy;
static EGLConfig cfg;

/* ------------------------------------------------------------------ */

static void drm_init_dev(int devnr, bool need_dumb, bool need_master)
{
    drmModeRes *res;
    char dev[64];
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
        if (rc < 0 || !has_dumb) {
            fprintf(stderr, "drmSetMaster() failed, X11 running?\n");
            exit(1);
        }
    }

    /* find connector (using first for now) */
    res = drmModeGetResources(fd);
    if (res == NULL) {
        fprintf(stderr, "drmModeGetResources() failed\n");
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
}

static void drm_draw_dumb_fb(void)
{
    int x, y;
    uint8_t *ptr;

    for (y = 0; y < creq.height; y++) {
        for (x = 0; x < creq.width; x++) {
            ptr = fbmem + creq.pitch * y + 4 * x;
            ptr[0] = 0;         /* blue  */
            ptr[1] = x & 0xff;  /* green */
            ptr[2] = y & 0xff;  /* red   */
            ptr[3] = 0;         /* alpha */
        }
    }
    drmModeDirtyFB(fd, fb_id, 0, 0);
}

/* ------------------------------------------------------------------ */

static void drm_init_egl(void)
{
    static const char *exts[] = {
        "EGL_KHR_surfaceless_context",
        "EGL_KHR_surfaceless_opengl",
    };
    static const EGLint conf_att[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,   5,
        EGL_GREEN_SIZE, 5,
        EGL_BLUE_SIZE,  5,
        EGL_ALPHA_SIZE, 0,
        EGL_NONE,
    };
    EGLint major, minor;
    EGLBoolean b;
    EGLint i, n;

    gbm = gbm_create_device(fd);
    if (!gbm) {
        fprintf(stderr, "egl: gbm_create_device failed\n");
        exit(1);
    }

    dpy = eglGetDisplay(gbm);
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

    for (i = 0; i < sizeof(exts)/sizeof(exts[0]); i++) {
        if (epoxy_has_egl_extension(dpy, exts[i]))
            continue;
        fprintf(stderr, "egl: %s not supported\n", exts[i]);
        exit(1);
    }
}

static void drm_init_egl_fb(void)
{
    EGLImageKHR image;
    struct gbm_bo *bo;
    GLuint color_rb, depth_rb;

    bo = gbm_bo_create(gbm, mode->hdisplay, mode->vdisplay,
                       GBM_BO_FORMAT_XRGB8888,
                       GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    image = eglCreateImageKHR(dpy, NULL, EGL_NATIVE_PIXMAP_KHR, bo, NULL);

    glGenFramebuffers(1, &fb_id);
    glBindFramebuffer(GL_FRAMEBUFFER_EXT, fb_id);

    /* Set up render buffer... */
    glGenRenderbuffers(1, &color_rb);
    glBindRenderbuffer(GL_RENDERBUFFER_EXT, color_rb);
    glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, image);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                 GL_RENDERBUFFER_EXT, color_rb);

    /* and depth buffer */
    glGenRenderbuffers(1, &depth_rb);
    glBindRenderbuffer(GL_RENDERBUFFER_EXT, depth_rb);
    glRenderbufferStorage(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT,
                          mode->hdisplay, mode->vdisplay);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
                                 GL_RENDERBUFFER_EXT, depth_rb);

    if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "opengl framebuffer setup failed\n");
        exit(1);
    }
}

static void drm_draw_egl_fb(void)
{
    glViewport(0, 0, mode->hdisplay, mode->vdisplay);
    glClearColor(0.7, 0, 0, 0); /* red */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glFinish();
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
            "\n");
}

int main(int argc, char **argv)
{
    int card = 0;
    bool gl = false;
    int c;

    for (;;) {
        c = getopt(argc, argv, "hgc:");
        if (c == -1)
            break;
        switch (c) {
        case 'c':
            card = atoi(optarg);
            break;
        case 'g':
            gl = true;
            break;
        case 'h':
            usage(stdout);
            exit(0);
        default:
            usage(stderr);
            exit(1);
        }
    }

    if (gl) {
        drm_init_dev(card, true, true);
        drm_init_egl();
        drm_init_egl_fb();
        drm_draw_egl_fb();
    } else {
        drm_init_dev(card, false, true);
        drm_init_dumb_fb();
        drm_draw_dumb_fb();
    }
    drm_show_fb();
    sleep(3);
    drm_fini_dev();
    return 0;
}
