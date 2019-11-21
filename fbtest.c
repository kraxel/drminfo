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

#include <cairo.h>
#include <pixman.h>

#include "fbtools.h"
#include "logind.h"
#include "ttytools.h"
#include "render.h"
#include "image.h"

/* ------------------------------------------------------------------ */

/* cairo */
static cairo_surface_t *cs;

/* user options */
static cairo_surface_t *image;

/* ------------------------------------------------------------------ */

static void fb_draw(bool autotest)
{
    char info1[80];
    char info2[80];
    char info3[80];
    cairo_t *cr;

    snprintf(info1, sizeof(info1), "mode: %dx%d", fb_var.xres, fb_var.yres);
    snprintf(info2, sizeof(info2), "%d bpp, rgba %d,%d,%d,%d",
             fb_var.bits_per_pixel,
             fb_var.red.length,
             fb_var.green.length,
             fb_var.blue.length,
             fb_var.transp.length);
    snprintf(info3, sizeof(info3), "fb driver: %s", fb_fix.id);

    cr = cairo_create(cs);
    if (image) {
        render_image(cr, fb_var.xres, fb_var.yres, image);
    } else {
        render_test(cr, fb_var.xres, fb_var.yres, info1, info2,
                    autotest ? NULL : info3);
    }
    cairo_destroy(cr);
}

/* ------------------------------------------------------------------ */

static void usage(FILE *fp)
{
    fprintf(fp,
            "\n"
            "usage: fbtest [ options ]\n"
            "\n"
            "options:\n"
            "  -h         print this\n"
            "  -a         autotest mode (don't print hardware info)\n"
            "  -f <nr>    pick framebuffer\n"
            "  -s <secs>  set sleep time (default: 60)\n"
            "  -i <file>  load and display image <file>\n"
            "\n");
}

int main(int argc, char **argv)
{
    int framebuffer = 0;
    int secs = 60;
    bool autotest = false;
    int c;

    for (;;) {
        c = getopt(argc, argv, "has:i:f:");
        if (c == -1)
            break;
        switch (c) {
        case 'a':
            autotest = true;
            break;
        case 'f':
            framebuffer = atoi(optarg);
            break;
        case 's':
            secs = atoi(optarg);
            break;
        case 'i':
            image = load_image(optarg);
            break;
        case 'h':
            usage(stdout);
            exit(0);
        default:
            usage(stderr);
            exit(1);
        }
    }

#if 0
    /* logind doesn't support framebuffers it seems ... */
    logind_init();
#endif
    fb_init(framebuffer);
    cs = cairo_image_surface_create_for_data(fb_mem + fb_mem_offset,
                                             fb_format,
                                             fb_var.xres,
                                             fb_var.yres,
                                             fb_fix.line_length);
    fb_draw(autotest);

    if (autotest)
        fprintf(stdout, "---ok---\n");
    tty_raw();
    kbd_wait(secs);
    kbd_read();
    tty_restore();

    fb_fini();
    logind_fini();
    return 0;
}
