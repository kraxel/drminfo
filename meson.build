project('drminfo', 'c')

# pkg-config deps
libdrm_dep    = dependency('libdrm')
gbm_dep       = dependency('gbm')
epoxy_dep     = dependency('epoxy')
cairo_dep     = dependency('cairo')
cairo_gl_dep  = dependency('cairo-gl')
pixman_dep    = dependency('pixman-1')
gtk3_dep      = dependency('gtk+-3.0')

# libjpeg dep
jpeg_dep      = declare_dependency(link_args : '-ljpeg')

drminfo_srcs  = [ 'drminfo.c', 'drmtools.c' ]
drmtest_srcs  = [ 'drmtest.c', 'drmtools.c', 'render.c', 'image.c' ]
gtktest_srcs  = [ 'gtktest.c', 'render.c', 'image.c' ]

drminfo_deps  = [ libdrm_dep, cairo_dep, pixman_dep ]
drmtest_deps  = [ libdrm_dep, gbm_dep, epoxy_dep,
                  cairo_dep, cairo_gl_dep, pixman_dep, jpeg_dep ]
gtktest_deps  = [ gtk3_dep,
                  cairo_dep, cairo_gl_dep, pixman_dep, jpeg_dep ]

executable('drminfo',
           sources      : drminfo_srcs,
           dependencies : drminfo_deps,
           install      : true)
executable('drmtest',
           sources      : drmtest_srcs,
           dependencies : drmtest_deps,
           install      : true)
executable('gtktest',
           sources      : gtktest_srcs,
           dependencies : gtktest_deps,
           install      : false)
