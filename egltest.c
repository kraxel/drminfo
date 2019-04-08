#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <assert.h>

#include <gbm.h>
#include <epoxy/gl.h>
#include <epoxy/egl.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cairo.h>
#include <pixman.h>

#include "drmtools.h"
#include "logind.h"
#include "ttytools.h"

/* ------------------------------------------------------------------ */

static void egl_print_info(void)
{
    printf("version:  %s\n", glGetString(GL_VERSION));
    printf("vendor:   %s\n", glGetString(GL_VENDOR));
    printf("renderer: %s\n", glGetString(GL_RENDERER));
}

static void egl_draw(void)
{
    /* dark gray background */
    glClearColor(0.2, 0.2, 0.2, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    /* draw rectangle */
    glBegin(GL_TRIANGLES);
    glColor3f(0.0, 0.0, 1.0);  /* blue */
    glVertex2f(-0.9, -0.9);
    glColor3f(0.0, 1.0, 0.0);  /* green */
    glVertex2f(0.9, 0.9);
    glColor3f(1.0, 0.0, 0.0);  /* red */
    glVertex2f(0.9, -0.9);
    glEnd();

    /* done */
    glFlush();
}

/* ------------------------------------------------------------------ */

static void usage(FILE *fp)
{
    fprintf(fp,
            "\n"
            "usage: egltest [ options ]\n"
            "\n"
            "options:\n"
            "  -h         print this\n"
            "  -a         autotest mode\n"
            "  -c <nr>    pick card\n"
            "  -s <secs>  set sleep time (default: 60)\n"
            "  -i         print device info\n"
            "\n");
}

int main(int argc, char **argv)
{
    int card = 0;
    int secs = 60;
    char *output = NULL;
    char *modename = NULL;
    bool printinfo = false;
    bool autotest = false;
    char buf[32];
    int c;

    for (;;) {
        c = getopt(argc, argv, "haic:s:");
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
            printinfo = true;
            break;
        case 'a':
            autotest = true;
            break;
        case 'h':
            usage(stdout);
            exit(0);
        default:
            usage(stderr);
            exit(1);
        }
    }

    logind_init();
    drm_init_dev(card, output, modename, false);
    drm_setup_egl();

    if (printinfo) {
        egl_print_info();
        goto done;
    }

    egl_draw();
    drm_egl_flush_display();

    if (autotest)
        fprintf(stdout, "---ok---\n");

    tty_raw();
    kbd_wait(secs);
    read(0, buf, sizeof(buf));
    tty_restore();

done:
    drm_fini_dev();
    logind_fini();
    return 0;
}
