#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>

#include <cairo.h>
#include <gtk/gtk.h>

#include "render.h"
#include "image.h"
#include "complete.h"

cairo_surface_t *image;

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr,
                              gpointer user_data)
{
    char info1[80], info2[80];
    int width, height;

    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);

    if (image) {
        render_image(cr, width, height, image);
    } else {
        snprintf(info1, sizeof(info1), "gtktest");
        snprintf(info2, sizeof(info2), "%dx%d", width, height);
        render_test(cr, width, height, info1, info2, "");
    }
    return FALSE;
}

static void usage(FILE *fp)
{
    fprintf(fp,
            "\n"
            "usage: gtktest [ options ]\n"
            "\n"
            "options:\n"
            "  -h | --help          print this\n"
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
        .name    = "complete-bash",
        .has_arg = false,
        .val     = OPT_LONG_COMP_BASH,
    },{

        /* --- with argument --- */
        .name    = "image",
        .has_arg = true,
        .val     = 'i',
    },{
        /* end of list */
    }
};

int main(int argc, char *argv[])
{
    GtkWidget *window;
    GtkWidget *darea;
    char *ifile = NULL;
    bool gtk_ok;
    int c;

    gtk_ok = gtk_init_check(&argc, &argv);

    for (;;) {
        c = getopt_long(argc, argv, "hi:", long_opts, NULL);
        if (c == -1)
            break;
        switch (c) {
        case 'i':
            ifile = optarg;
            break;
        case OPT_LONG_COMP_BASH:
            complete_bash("gtktest", long_opts);
            exit(0);
        case 'h':
            usage(stdout);
            exit(0);
        default:
            usage(stderr);
            exit(1);
        }
    }

    if (!gtk_ok) {
        fprintf(stderr, "gtk init failed\n");
        exit(1);
    }

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    darea = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(window), darea);

    g_signal_connect(G_OBJECT(darea), "draw",
                     G_CALLBACK(on_draw_event), NULL);
    g_signal_connect(window, "destroy",
                     G_CALLBACK(gtk_main_quit), NULL);

    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(window), 640, 480);
    gtk_window_set_title(GTK_WINDOW(window), "gtktest");

    if (ifile) {
        fprintf(stderr, "loading %s ...\n", argv[1]);
        image = load_image(ifile);
    }

    gtk_widget_show_all(window);

    gtk_main();
    return 0;
}
