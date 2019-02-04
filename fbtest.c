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
#include "ttytools.h"
#include "render.h"
#include "image.h"

/* ------------------------------------------------------------------ */

/* cairo */
static cairo_surface_t *cs;

/* user options */
static cairo_surface_t *image;

/* ------------------------------------------------------------------ */

static void fb_draw(const char *text)
{
    char info[80];
    cairo_t *cr;

    snprintf(info, sizeof(info), "fbtest: %dx%d", fb_var.xres, fb_var.yres);
    cr = cairo_create(cs);
    if (image) {
        render_image(cr, fb_var.xres, fb_var.yres, image);
    } else {
        render_test(cr, fb_var.xres, fb_var.yres, info, text);
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
            "  -f <nr>    pick framebuffer\n"
            "  -s <secs>  set sleep time\n"
            "  -i <file>  load and display image <file>\n"
            "\n");
}

int main(int argc, char **argv)
{
    int framebuffer = 0;
    int secs = 60;
    char buf[32];
    char text[128];
    int c;

    for (;;) {
        c = getopt(argc, argv, "hs:i:f:");
        if (c == -1)
            break;
        switch (c) {
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

    fb_init(framebuffer);
    cs = cairo_image_surface_create_for_data(fb_mem + fb_mem_offset,
                                             fb_format,
                                             fb_var.xres,
                                             fb_var.yres,
                                             fb_fix.line_length);
    snprintf(text, sizeof(text), "fbdev, %d bpp",
             fb_var.bits_per_pixel);
    fb_draw(text);

    tty_raw();
    kbd_wait(secs);
    read(0, buf, sizeof(buf));
    tty_restore();

    fb_fini();
    return 0;
}
