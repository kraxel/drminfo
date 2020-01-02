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
#include "logind.h"
#include "complete.h"

/* ------------------------------------------------------------------ */

static void usage(FILE *fp)
{
    fprintf(fp,
            "\n"
            "usage: fbinfo [ options ]\n"
            "\n"
            "options:\n"
            "  -h | --help         print this\n"
            "  -f | --fbdev <nr>   pick framebuffer\n"
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
        .name    = "complete-bash",
        .has_arg = false,
        .val     = OPT_LONG_COMP_BASH,
    },{

        /* --- with argument --- */
        .name    = "fbdev",
        .has_arg = true,
        .val     = 'f',
    },{
        /* end of list */
    }
};

int main(int argc, char **argv)
{
    int framebuffer = 0;
    int c;

    for (;;) {
        c = getopt_long(argc, argv, "hf:", long_opts, NULL);
        if (c == -1)
            break;
        switch (c) {
        case 'f':
            framebuffer = atoi(optarg);
            break;
        case OPT_LONG_COMP_BASH:
            complete_bash("fbinfo", long_opts);
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
    fb_query(framebuffer);
    logind_fini();

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
