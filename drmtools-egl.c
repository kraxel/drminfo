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

#include <gbm.h>
#include <epoxy/gl.h>
#include <epoxy/egl.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cairo.h>
#include <pixman.h>

#include "drmtools.h"

/* ------------------------------------------------------------------ */

static struct gbm_device *gbm_dev;
static struct gbm_surface *gbm_surface;
static EGLDisplay dpy;
static EGLConfig cfg;
static EGLContext ctx;
static EGLSurface surface;

/* ------------------------------------------------------------------ */

int drm_setup_egl(void)
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

    gbm_dev = gbm_create_device(drm_fd);
    if (!gbm_dev) {
        fprintf(stderr, "egl: gbm_create_device failed\n");
        return -1;
    }

    gbm_surface = gbm_surface_create(gbm_dev,
                                     drm_mode->hdisplay,
                                     drm_mode->vdisplay,
                                     GBM_FORMAT_XRGB8888,
                                     GBM_BO_USE_RENDERING);
    if (!gbm_surface) {
        fprintf(stderr, "egl: gbm_create_surface failed\n");
        return -1;
    }

#ifdef EGL_MESA_platform_gbm
    dpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_MESA, gbm_dev, NULL);
#else
    dpy = eglGetDisplay(gbm_dev);
#endif
    if (dpy == EGL_NO_DISPLAY) {
        fprintf(stderr, "egl: eglGetDisplay failed\n");
        return -1;
    }

    b = eglInitialize(dpy, &major, &minor);
    if (b == EGL_FALSE) {
        fprintf(stderr, "egl: eglInitialize failed\n");
        return -1;
    }

    b = eglBindAPI(EGL_OPENGL_API);
    if (b == EGL_FALSE) {
        fprintf(stderr, "egl: eglBindAPI failed\n");
        return -1;
    }

    b = eglChooseConfig(dpy, conf_att, &cfg, 1, &n);
    if (b == EGL_FALSE || n != 1) {
        fprintf(stderr, "egl: eglChooseConfig failed\n");
        return -1;
    }

    ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_att);
    if (ctx == EGL_NO_CONTEXT) {
        fprintf(stderr, "egl: eglCreateContext failed\n");
        return -1;
    }

    b = eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx);
    if (b == EGL_FALSE) {
        fprintf(stderr, "egl: eglMakeCurrent(EGL_NO_SURFACE) failed\n");
        return -1;
    }

    surface = eglCreateWindowSurface(dpy, cfg,
                                     (EGLNativeWindowType)gbm_surface,
                                     NULL);
    if (!surface) {
        fprintf(stderr, "egl: eglCreateWindowSurface failed\n");
        return -1;
    }

    b = eglMakeCurrent(dpy, surface, surface, ctx);
    if (b == EGL_FALSE) {
        fprintf(stderr, "egl: eglMakeCurrent(surface) failed\n");
        return -1;
    }
    return 0;
}

static int fbid;
static struct gbm_bo *bo;

void drm_egl_flush_display(void)
{
    uint32_t handle, stride, newfb;
    struct gbm_bo *newbo;
    int rc;

    eglSwapBuffers(dpy, surface);

    newbo = gbm_surface_lock_front_buffer(gbm_surface);
    if (!newbo) {
        fprintf(stderr, "egl: gbm_surface_lock_front_buffer failed\n");
        return;
    }
    handle = gbm_bo_get_handle(newbo).u32;
    stride = gbm_bo_get_stride(newbo);

    drmModeAddFB(drm_fd, drm_mode->hdisplay, drm_mode->vdisplay, 24, 32,
                 stride, handle, &newfb);
    rc = drmModeSetCrtc(drm_fd, drm_enc->crtc_id, newfb, 0, 0,
                        &drm_conn->connector_id, 1,
                        &drm_conn->modes[0]);
    if (rc < 0) {
        fprintf(stderr, "egl: drmModeSetCrtc() failed\n");
        exit(1);
    }

    if (fbid) {
        drmModeRmFB(drm_fd, fbid);
    }
    fbid = newfb;

    if (bo) {
        gbm_surface_release_buffer(gbm_surface, bo);
    }
    bo = newbo;
}
