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

#include "fbtools.h"

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
            "\n");
}

int main(int argc, char **argv)
{
    int framebuffer = 0;
    int c;

    for (;;) {
        c = getopt(argc, argv, "hf:");
        if (c == -1)
            break;
        switch (c) {
        case 'f':
            framebuffer = atoi(optarg);
            break;
        case 'h':
            usage(stdout);
            exit(0);
        default:
            usage(stderr);
            exit(1);
        }
    }

    fb_query(framebuffer);

    fprintf(stderr, "fb%d: %s, %dx%d, %d bpp, r/g/b/a %d/%d/%d/%d\n",
            framebuffer,
            fb_fix.id,
            fb_var.xres, fb_var.yres,
            fb_var.bits_per_pixel,
            fb_var.red.length,
            fb_var.green.length,
            fb_var.blue.length,
            fb_var.transp.length);
    return 0;
}