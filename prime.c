#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <gbm.h>

#include "logind.h"
#include "complete.h"

#define TEST_WIDTH  640
#define TEST_HEIGHT 480
#define TEST_SIZE   (TEST_WIDTH * TEST_HEIGHT * 4)

#define DEV_COUNT 16

struct dev {
    int index, fd;
    char devname[64];
    drmVersion *ver;
    uint64_t prime;
    bool import;
    bool export;
    struct gbm_device *gbm;
};

/* ------------------------------------------------------------------ */

#define INDENT_WIDTH  4
#define NAME_WIDTH   16

static void print_head(const char *name)
{
    fprintf(stderr, "%*s%s\n", INDENT_WIDTH, "", name);
}

static void print_caps(const char *name, bool available)
{
    fprintf(stderr, "%*s%-*s: %s\n",
            INDENT_WIDTH * 2, "",
            NAME_WIDTH, name,
            available ? "yes" : "no");
}

static void print_test(const char *name, bool failed, int err)
{
    fprintf(stderr, "%*s%-*s: %s",
            INDENT_WIDTH * 2, "",
            NAME_WIDTH, name,
            !failed ? "OK" : "FAILED");
    if (failed && err)
        fprintf(stderr, " (%s)", strerror(err));
    fprintf(stderr, "\n");
}

/* ------------------------------------------------------------------ */

static struct dev *drm_init_dev(int card)
{
    struct dev *dev;
    int rc;

    dev = malloc(sizeof(*dev));
    memset(dev, 0, sizeof(*dev));
    dev->index = card;

    /* open device */
    snprintf(dev->devname, sizeof(dev->devname),
             DRM_DEV_NAME, DRM_DIR_NAME, dev->index);
    if (access(dev->devname, F_OK) < 0)
        return NULL;
    dev->fd = device_open(dev->devname);
    if (dev->fd < 0)
        return NULL;

    dev->ver = drmGetVersion(dev->fd);
    fprintf(stderr, "%s:\n", dev->devname);
    fprintf(stderr, "%*sdriver: %s, v%d.%d.%d\n",
            INDENT_WIDTH, "", dev->ver->name,
            dev->ver->version_major, dev->ver->version_minor,
            dev->ver->version_patchlevel);

    rc = drmGetCap(dev->fd, DRM_CAP_PRIME, &dev->prime);
    if (rc < 0) {
        fprintf(stderr, "drmGetCap(DRM_CAP_PRIME): %s\n", strerror(errno));
        exit(1);
    }
    dev->import = dev->prime & DRM_PRIME_CAP_IMPORT;
    dev->export = dev->prime & DRM_PRIME_CAP_EXPORT;

    print_head("device capabilities");
    print_caps("prime import", dev->import);
    print_caps("prime export", dev->export);

    return dev;
}

static int drm_dumb_buf(int fd)
{
    struct drm_mode_create_dumb creq = {
        .width  = TEST_WIDTH,
        .height = TEST_HEIGHT,
        .bpp    = 32,
    };
    int rc;

    rc = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    print_test("create buffer", rc < 0, errno);
    if (rc < 0) {
        return rc;
    } else {
        return creq.handle;
    }
}

static int drm_export_buf(int fd, int handle)
{
    int dmabuf;
    int rc;

    rc = drmPrimeHandleToFD(fd, handle, 0, &dmabuf);
    print_test("export buffer", rc < 0, errno);
    if (rc < 0) {
        return rc;
    } else {
        return dmabuf;
    }
}

/* ------------------------------------------------------------------ */

static void dmabuf_mmap(int dmabuf)
{
    void *ptr;

    ptr = mmap(NULL, TEST_SIZE, PROT_READ, MAP_SHARED, dmabuf, 0);
    print_test("mmap dmabuf", ptr == MAP_FAILED, errno);
    if (ptr != MAP_FAILED) {
        munmap(ptr, TEST_SIZE);
    }
}

/* ------------------------------------------------------------------ */

static struct gbm_device *gbm_init(int card)
{
    struct gbm_device *gbm;

    gbm = gbm_create_device(card);
    print_test("create gbm dev", !gbm, 0);
    return gbm;
}

static void gbm_test(struct gbm_device *gbm, bool export)
{
    struct gbm_bo *bo;
    int dmabuf;

    bo = gbm_bo_create(gbm, TEST_WIDTH, TEST_HEIGHT,
                       GBM_FORMAT_XRGB8888,
                       0);
    print_test("create gbm bo", !bo, 0);
    if (!bo)
        return;

    if (!export)
        goto done_bo;
    dmabuf = gbm_bo_get_fd(bo);
    print_test("export gbm bo", dmabuf < 0, 0);
    if (dmabuf < 0)
        goto done_bo;

    dmabuf_mmap(dmabuf);
    close(dmabuf);

done_bo:
    gbm_bo_destroy(bo);
}

static void gbm_export_import(struct dev *ex,
                              struct dev *im)
{
    struct gbm_bo *bo_ex, *bo_im;
    struct gbm_import_fd_data import;
    bool failed = true;
    int dmabuf;

    fprintf(stderr, "    %s (%d) -> %s (%d)\n",
            ex->ver->name, ex->index,
            im->ver->name, im->index);

    bo_ex = gbm_bo_create(ex->gbm, TEST_WIDTH, TEST_HEIGHT,
                          GBM_FORMAT_XRGB8888,
                          0);
    if (!bo_ex)
        return;

    dmabuf = gbm_bo_get_fd(bo_ex);
    if (dmabuf < 0)
        goto done1;

    import.fd = dmabuf;
    import.width = TEST_WIDTH;
    import.height = TEST_HEIGHT;
    import.stride = TEST_WIDTH * 4;
    import.format = GBM_FORMAT_XRGB8888;
    bo_im = gbm_bo_import(im->gbm, GBM_BO_IMPORT_FD, &import, 0);
    if (!bo_im)
        goto done2;

    /* passed */
    failed = false;

    /* cleanup */
    gbm_bo_destroy(bo_im);
done2:
    close(dmabuf);
done1:
    gbm_bo_destroy(bo_ex);

    print_test("transfer dmabuf", failed, 0);
}

/* ------------------------------------------------------------------ */

static void usage(FILE *fp)
{
    fprintf(fp,
            "\n"
            "usage: prime [ options ]\n"
            "\n"
            "options:\n"
            "  -h | --help        print this\n"
            "  -l | --list-cards  list cards\n"
            "\n");
}

enum {
    OPT_LONG_COMP_BASH = 0x100,
};

static struct option long_opts[] = {
    {
        /* --- no argument --- */
        .name    = "help",
        .has_arg = false,
        .val     = 'h',
    },{
        .name    = "list-cards",
        .has_arg = false,
        .val     = 'l',
    },{
        .name    = "complete-bash",
        .has_arg = false,
        .val     = OPT_LONG_COMP_BASH,
    },{
        /* end of list */
    }
};

int main(int argc, char **argv)
{
    struct dev *devs[DEV_COUNT] = {};
    int handle, dmabuf, c, i, e;
    bool list = false;

    for (;;) {
        c = getopt_long(argc, argv, "hl", long_opts, NULL);
        if (c == -1)
            break;
        switch (c) {
        case 'l':
            list = true;
            break;
        case OPT_LONG_COMP_BASH:
            complete_bash("prime", long_opts);
            exit(0);
        case 'h':
            usage(stdout);
            exit(0);
        default:
            usage(stderr);
            exit(1);
        }
    }

    logind_init();

    for (i = 0; i < DEV_COUNT; i++) {
        devs[i] = drm_init_dev(i);
        if (!devs[i])
            break;
        if (list) {
            close(devs[i]->fd);
            continue;
        }

        print_head("test dumb buffer (ioctl)");
        handle = drm_dumb_buf(devs[i]->fd);
        if (handle >= 0 && devs[i]->export) {
            dmabuf = drm_export_buf(devs[i]->fd, handle);
            if (dmabuf >= 0) {
                dmabuf_mmap(dmabuf);
                close(dmabuf);
            }
        }

        print_head("test gdm buffer (mesa-libgbm)");
        devs[i]->gbm = gbm_init(devs[i]->fd);
        if (devs[i]->gbm)
            gbm_test(devs[i]->gbm, devs[i]->export);

        fprintf(stderr, "\n");
    }

    if (!list) {
        fprintf(stderr, "dma-buf transfer tests\n");
        for (e = 0; e < DEV_COUNT; e++) {
            if (!devs[e] ||
                !devs[e]->gbm ||
                !devs[e]->export)
                continue;
            for (i = 0; i < DEV_COUNT; i++) {
                if (!devs[i] ||
                    !devs[i]->gbm ||
                    !devs[i]->import)
                continue;
                gbm_export_import(devs[e], devs[i]);
            }
        }
    }

    logind_fini();
    return 0;
}
