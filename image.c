#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <endian.h>

#include <jpeglib.h>
#include <jerror.h>
#include <cairo.h>

#include "image.h"

cairo_surface_t *load_jpeg(const char* filename)
{
    struct jpeg_decompress_struct info;
    struct jpeg_error_mgr err;
    cairo_surface_t *surface;
    uint8_t *rowptr[1];
    FILE* file;

    file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "open %s: %s\n", filename, strerror(errno));
        exit(1);
    }

    info.err = jpeg_std_error(&err);
    jpeg_create_decompress(&info);

    jpeg_stdio_src(&info, file);
    jpeg_read_header(&info, TRUE);
#if __BYTE_ORDER == __LITTLE_ENDIAN
    info.out_color_space = JCS_EXT_BGRX;
#else
    info.out_color_space = JCS_EXT_XRGB;
#endif
    jpeg_start_decompress(&info);

    surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, /* 32bpp, native endian */
                                         info.output_width,
                                         info.output_height);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "cairo_image_surface_create failed\n");
        exit(1);
    }

    while (info.output_scanline < info.output_height) {
        rowptr[0] = (uint8_t *)cairo_image_surface_get_data(surface) +
            cairo_image_surface_get_stride(surface) * info.output_scanline;
        jpeg_read_scanlines(&info, rowptr, 1);
    }
    jpeg_finish_decompress(&info);

    fclose(file);
    return surface;
}
