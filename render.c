#include <cairo.h>

static int pad = 10;

static void render_color_bar(cairo_t *cr, int x, int y, int w, int h,
                             double r, double g, double b, const char *name)
{
    cairo_font_extents_t ext;
    cairo_pattern_t *gr;

    gr = cairo_pattern_create_linear(x, y+h/2, w, y+h/2);
    cairo_pattern_add_color_stop_rgb(gr, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgb(gr, 1, r, g, b);
    cairo_rectangle (cr, x, y, w, h);
    cairo_set_source (cr, gr);
    cairo_fill (cr);
    cairo_pattern_destroy(gr);

    cairo_set_source_rgb(cr, r, g, b);
    cairo_select_font_face(cr, "mono",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, h - 2*pad);
    cairo_font_extents (cr, &ext);
    cairo_move_to(cr, x + pad, y + pad + ext.ascent);
    cairo_show_text(cr, name);
}

void render_test(cairo_t *cr, int width, int height)
{
    int bar = 100;

    while (6 * bar + 2 * pad > height &&
           bar > 4 * pad)
        bar -= 10;

    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_rectangle (cr, 0, 0, width, height);
    cairo_fill (cr);

    cairo_set_line_width (cr, 1);
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_rectangle (cr, pad - 0.5, pad - 0.5, width - 2*pad + 1, 6*bar + 1);
    cairo_stroke (cr);

    render_color_bar(cr, pad, bar * 0 + pad, width - 2*pad, bar, 1, 0, 0, "red");
    render_color_bar(cr, pad, bar * 1 + pad, width - 2*pad, bar, 1, 1, 0, "yellow");
    render_color_bar(cr, pad, bar * 2 + pad, width - 2*pad, bar, 0, 1, 0, "green");
    render_color_bar(cr, pad, bar * 3 + pad, width - 2*pad, bar, 0, 1, 1, "cyan");
    render_color_bar(cr, pad, bar * 4 + pad, width - 2*pad, bar, 0, 0, 1, "blue");
    render_color_bar(cr, pad, bar * 5 + pad, width - 2*pad, bar, 1, 0, 1, "magenta");
}


