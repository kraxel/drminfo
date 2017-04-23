#!/bin/sh

awk '/define DRM.*fourcc_code.*\[.*\]/ {
	printf "    {\n";
        printf "        .name   = FOURCC_NAME(%s),\n", $2;
        printf "        .fields = \"%s\",\n", $9;
        printf "        .bits   = \"%s\",\n", $10;
        printf "        .bpp    = ,\n";
        printf "        .fourcc = %s,\n", $2;
        printf "        .cairo  = CAIRO_FORMAT_INVALID,\n", $2;
        printf "    },\n";
}' /usr/include/drm/drm_fourcc.h
