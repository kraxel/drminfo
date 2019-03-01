#include <stdio.h>
#include <stddef.h>
#include <cairo.h>

#include "render.h"

static int pad = 15;

static void render_color_bar(cairo_t *cr, int x, int y, int w, int h,
                             double r, double g, double b,
                             const char *l1, const char *l2, const char *l3)
{
    cairo_font_extents_t ext;
    cairo_pattern_t *gr;
    int lines;

    gr = cairo_pattern_create_linear(x, y+h/2, w, y+h/2);
    cairo_pattern_add_color_stop_rgb(gr, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgb(gr, 1, r, g, b);
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source(cr, gr);
    cairo_fill(cr);
    cairo_pattern_destroy(gr);

    cairo_set_source_rgb(cr, r, g, b);
    cairo_select_font_face(cr, "Liberation Mono",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);

    lines = 1;
    if (l2) {
        cairo_set_source_rgb(cr, 1, 1, 1);
        lines++;
        if (l3)
            lines++;
    }

    cairo_set_font_size(cr, (h - 2*pad) / lines);
    cairo_font_extents(cr, &ext);
    cairo_move_to(cr, x + pad, y + pad + ext.ascent);
    cairo_show_text(cr, l1);
    if (l2) {
        cairo_move_to(cr, x + pad, y + pad + ext.ascent + ext.height);
        cairo_show_text(cr, l2);
        if (l3) {
            cairo_move_to(cr, x + pad, y + pad + ext.ascent + ext.height * 2);
            cairo_show_text(cr, l3);
        }
    }
}

void render_test(cairo_t *cr, int width, int height,
                 const char *l1, const char *l2, const char *l3)
{
    int bar = 120;

    while (7 * bar + 2 * pad > height &&
           bar > 4 * pad)
        bar -= 10;

    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    cairo_set_line_width(cr, 2);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_rectangle(cr, pad - 1, pad - 1, width - 2*pad + 2, 7*bar + 2);
    cairo_stroke(cr);

    render_color_bar(cr, pad, bar * 0 + pad, width - 2*pad, bar,
                     0.6, 0.6, 0.6, l1, l2, l3);
    render_color_bar(cr, pad, bar * 1 + pad, width - 2*pad, bar, 1, 0, 0,
                     "red", NULL, NULL);
    render_color_bar(cr, pad, bar * 2 + pad, width - 2*pad, bar, 1, 1, 0,
                     "yellow", NULL, NULL);
    render_color_bar(cr, pad, bar * 3 + pad, width - 2*pad, bar, 0, 1, 0,
                     "green", NULL, NULL);
    render_color_bar(cr, pad, bar * 4 + pad, width - 2*pad, bar, 0, 1, 1,
                     "cyan", NULL, NULL);
    render_color_bar(cr, pad, bar * 5 + pad, width - 2*pad, bar, 0, 0, 1,
                     "blue", NULL, NULL);
    render_color_bar(cr, pad, bar * 6 + pad, width - 2*pad, bar, 1, 0, 1,
                     "magenta", NULL, NULL);

    cairo_show_page(cr);
}

void render_image(cairo_t *cr, int width, int height, cairo_surface_t *image)
{
    double xs, ys, dx, dy;
    int iw, ih;

    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    iw = cairo_image_surface_get_width(image);
    ih = cairo_image_surface_get_height(image);
    xs = (double)width / iw;
    ys = (double)height / ih;

    if (xs > ys) {
        xs = ys;
        dx = (width - xs * iw) / 2;
        dy = 0;
    } else {
        ys = xs;
        dx = 0;
        dy = (height - ys * ih) / 2;
    }

    cairo_translate(cr, dx, dy);
    cairo_scale(cr, xs, ys);
    cairo_set_source_surface(cr, image, 0, 0);
    cairo_rectangle(cr, 0, 0, iw, ih);
    cairo_fill(cr);

    cairo_show_page(cr);
}
