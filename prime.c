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

#define TEST_WIDTH  640
#define TEST_HEIGHT 480
#define TEST_SIZE   (TEST_WIDTH * TEST_HEIGHT * 4)

#define NAME_WIDTH  16

static void print_head(const char *name)
{
    fprintf(stderr, "   %s\n", name);
}

static void print_caps(const char *name, bool available)
{
    fprintf(stderr, "      %-*s: %s\n", NAME_WIDTH, name,
            available ? "yes" : "no");
}

static void print_test(const char *name, bool failed, int err)
{
    fprintf(stderr, "      %-*s: %s", NAME_WIDTH, name,
            !failed ? "OK" : "FAILED");
    if (failed && err)
        fprintf(stderr, " (%s)", strerror(err));
    fprintf(stderr, "\n");
}

/* ------------------------------------------------------------------ */

static int drm_init_dev(const char *devname, bool *import, bool *export)
{
    drmVersion *ver;
    uint64_t prime;
    int fd, rc;

    /* open device */
    fd = open(devname, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", devname, strerror(errno));
        exit(1);
    }

    ver = drmGetVersion(fd);
    fprintf(stderr, "%s:\n", devname);
    fprintf(stderr, "   driver: %s, v%d.%d.%d\n", ver->name,
            ver->version_major, ver->version_minor,
            ver->version_patchlevel);

    rc = drmGetCap(fd, DRM_CAP_PRIME, &prime);
    if (rc < 0) {
        fprintf(stderr, "drmGetCap(DRM_CAP_PRIME): %s\n", strerror(errno));
        exit(1);
    }
    *import = prime & DRM_PRIME_CAP_IMPORT;
    *export = prime & DRM_PRIME_CAP_EXPORT;

    print_head("device capabilities");
    print_caps("prime import", *import);
    print_caps("prime export", *export);

    return fd;
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

static void gbm_test(int card, bool export)
{
    struct gbm_device *gbm;
    struct gbm_bo *bo;
    int dmabuf;

    fprintf(stderr, "   test gdm buffer (mesa-libgbm)\n");
    gbm = gbm_create_device(card);
    print_test("create gbm dev", !gbm, 0);
    if (!gbm)
        return;

    bo = gbm_bo_create(gbm, TEST_WIDTH, TEST_HEIGHT,
                       GBM_FORMAT_XRGB8888,
                       0);
    print_test("create gbm bo", !bo, 0);
    if (!bo)
        goto done_gbm;

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
done_gbm:
    gbm_device_destroy(gbm);
}

static void gbm_export_import(int ex, int im)
{
    struct gbm_device *gbm_ex;
    struct gbm_device *gbm_im;
    struct gbm_bo *bo_ex, *bo_im;
    struct gbm_import_fd_data import;
    char devname[64];
    int card_ex, card_im, dmabuf;

    fprintf(stderr, "test export/import: card %d -> card %d\n", ex, im);

    snprintf(devname, sizeof(devname), DRM_DEV_NAME, DRM_DIR_NAME, ex);
    card_ex = open(devname, O_RDWR);
    gbm_ex = gbm_create_device(card_ex);
    if (!gbm_ex) {
        fprintf(stderr, "%s: gdm init (ex) failed\n", devname);
        exit(1);
    }

    snprintf(devname, sizeof(devname), DRM_DEV_NAME, DRM_DIR_NAME, im);
    card_im = open(devname, O_RDWR);
    gbm_im = gbm_create_device(card_im);
    if (!gbm_im) {
        fprintf(stderr, "%s: gdm init (im) failed\n", devname);
        exit(1);
    }

    bo_ex = gbm_bo_create(gbm_ex, TEST_WIDTH, TEST_HEIGHT,
                          GBM_FORMAT_XRGB8888,
                          0);
    print_test("create gbm bo", !bo_ex, 0);
    if (!bo_ex)
        exit(1);

    dmabuf = gbm_bo_get_fd(bo_ex);
    print_test("export gbm bo", dmabuf < 0, 0);
    if (dmabuf < 0)
        exit(1);

    import.fd = dmabuf;
    import.width = TEST_WIDTH;
    import.height = TEST_HEIGHT;
    import.stride = TEST_WIDTH * 4;
    import.format = GBM_FORMAT_XRGB8888;
    bo_im = gbm_bo_import(gbm_im, GBM_BO_IMPORT_FD, &import, 0);
    print_test("import gbm bo", !bo_im, 0);
    if (!bo_im) {
        exit(1);
    }
}

/* ------------------------------------------------------------------ */

static void usage(FILE *fp)
{
    fprintf(fp,
            "\n"
            "usage: prime [ options ]\n"
            "\n"
            "options:\n"
            "  -h         print this\n"
            "  -l         list cards\n"
            "  -e <nr>    export from this card\n"
            "  -i <nr>    import into this card\n"
            "\n");
}

int main(int argc, char **argv)
{
    char devname[64];
    bool import, export;
    int card, handle, dmabuf, c, i;
    int ex = -1;
    int im = -1;
    bool exarg = false;
    bool imarg = false;
    bool list = false;

    for (;;) {
        c = getopt(argc, argv, "hle:i:");
        if (c == -1)
            break;
        switch (c) {
        case 'l':
            list = true;
            break;
        case 'e':
            exarg = true;
            ex = atoi(optarg);
            break;
        case 'i':
            imarg = true;
            im = atoi(optarg);
            break;
        case 'h':
            usage(stdout);
            exit(0);
        default:
            usage(stderr);
            exit(1);
        }
    }

    for (i = 0;; i++) {
        snprintf(devname, sizeof(devname), DRM_DEV_NAME, DRM_DIR_NAME, i);
        if (access(devname, R_OK | W_OK) != 0)
            break;
        card = drm_init_dev(devname, &import, &export);
        if (list) {
            close(card);
            continue;
        }

        fprintf(stderr, "   test dumb buffer (ioctl)\n");
        handle = drm_dumb_buf(card);
        if (handle >= 0 && export) {
            dmabuf = drm_export_buf(card, handle);
            if (dmabuf >= 0) {
                dmabuf_mmap(dmabuf);
                close(dmabuf);
            }
        }

        gbm_test(card, export);

        if (export && !exarg && (ex == -1 || ex == im))
            ex = i;
        if (import && !imarg && (im == -1 || im == ex))
            im = i;

        close(card);
    }

    if (!list && ex != -1 && im != -1 && ex != im)
        gbm_export_import(ex, im);

    return 0;
}
