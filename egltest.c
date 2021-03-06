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
#include "drm-lease.h"
#include "logind.h"
#include "ttytools.h"
#include "complete.h"

/* ------------------------------------------------------------------ */

static void egl_print_info(void)
{
    printf("opengl version:  %s\n", glGetString(GL_VERSION));
    printf("shader version:  %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("vendor:          %s\n", glGetString(GL_VENDOR));
    printf("renderer:        %s\n", glGetString(GL_RENDERER));
}

static void egl_print_ext(void)
{
    char *ext = strdup((char*)glGetString(GL_EXTENSIONS));
    int i, len = strlen(ext);

    for (i = 0; i < len; i++)
        if (ext[i] == ' ')
            ext[i] = '\n';
    printf("%s", ext);
    free(ext);
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
            "  -h | --help            print this\n"
            "  -a | --autotest        autotest mode\n"
            "  -i | --info            print device info\n"
            "  -x | --exts            print extensions\n"
            "  -c | --card  <nr>      pick card\n"
            "  -s | --sleep <secs>    set sleep time (default: 60)\n"
            "       --lease <output>  get a drm lease for output\n"
            "\n");
}

enum {
    OPT_LONG_LEASE,
    OPT_LONG_COMP_BASH,
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
        .name    = "info",
        .has_arg = false,
        .val     = 'i',
    },{
        .name    = "exts",
        .has_arg = false,
        .val     = 'x',
    },{
        .name    = "complete-bash",
        .has_arg = false,
        .val     = OPT_LONG_COMP_BASH,
    },{

        /* --- with argument --- */
        .name    = "card",
        .has_arg = true,
        .val     = 'c',
    },{
        .name    = "sleep",
        .has_arg = true,
        .val     = 's',
    },{
        .name    = "lease",
        .has_arg = true,
        .val     = OPT_LONG_LEASE,
    },{
        /* end of list */
    }
};

int main(int argc, char **argv)
{
    int card = 0;
    int secs = 60;
    int lease_fd = -1;
    char *output = NULL;
    char *modename = NULL;
    bool printinfo = false;
    bool printext = false;
    bool autotest = false;
    int c;

    for (;;) {
        c = getopt_long(argc, argv, "haixc:s:", long_opts, NULL);
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
        case 'x':
            printext = true;
            break;
        case 'a':
            autotest = true;
            break;
        case OPT_LONG_LEASE:
            lease_fd = drm_lease(optarg);
            break;
        case OPT_LONG_COMP_BASH:
            complete_bash("egltest", long_opts);
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
    drm_init_dev(card, output, modename, false, lease_fd);
    drm_setup_egl();

    if (printinfo)
        egl_print_info();
    if (printext)
        egl_print_ext();
    if (printinfo || printext)
        goto done;

    egl_draw();
    drm_egl_flush_display();

    if (autotest)
        fprintf(stdout, "---ok---\n");

    tty_raw();
    kbd_wait(secs);
    kbd_read();
    tty_restore();

done:
    drm_fini_dev();
    logind_fini();
    return 0;
}
