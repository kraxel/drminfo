#include <stdio.h>
#include <cairo.h>
#include <gtk/gtk.h>

#include "render.h"
#include "image.h"

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

int main(int argc, char *argv[])
{
    GtkWidget *window;
    GtkWidget *darea;

    gtk_init(&argc, &argv);

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

    if (argv[1]) {
        fprintf(stderr, "loading %s ...\n", argv[1]);
        image = load_image(argv[1]);
    }

    gtk_widget_show_all(window);

    gtk_main();
    return 0;
}
