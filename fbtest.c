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
#include "complete.h"

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
            "  -h | --help          print this\n"
            "  -a | --autotest      autotest mode (don't print hardware info)\n"
            "  -f | --fbdev <nr>    pick framebuffer\n"
            "  -s | --sleep <secs>  set sleep time (default: 60)\n"
            "  -i | --image <file>  load and display image <file>\n"
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
        .name    = "autotest",
        .has_arg = false,
        .val     = 'a',
    },{
        .name    = "complete-bash",
        .has_arg = false,
        .val     = OPT_LONG_COMP_BASH,
    },{

        /* --- with argument --- */
        .name    = "fbdev",
        .has_arg = true,
        .val     = 'f',
    },{
        .name    = "sleep",
        .has_arg = true,
        .val     = 's',
    },{
        .name    = "image",
        .has_arg = true,
        .val     = 'i',
    },{
        /* end of list */
    }
};

int main(int argc, char **argv)
{
    int framebuffer = 0;
    int secs = 60;
    bool autotest = false;
    int c;

    for (;;) {
        c = getopt_long(argc, argv, "has:i:f:", long_opts, NULL);
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
        case OPT_LONG_COMP_BASH:
            complete_bash("fbtest", long_opts);
            exit(0);
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
