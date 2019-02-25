project('drminfo', 'c', default_options : [ 'c_std=c99' ] )

# tweak warnings
add_global_arguments('-D_POSIX_SOURCE=1', language : 'c')
add_global_arguments('-D_GNU_SOURCE=1', language : 'c')

# pkg-config deps
libdrm_dep    = dependency('libdrm')
gbm_dep       = dependency('gbm')
epoxy_dep     = dependency('epoxy')
cairo_dep     = dependency('cairo')
pixman_dep    = dependency('pixman-1')
gtk3_dep      = dependency('gtk+-3.0')

# libjpeg dep
jpeg_dep      = declare_dependency(link_args : '-ljpeg')

drminfo_srcs  = [ 'drminfo.c', 'drmtools.c' ]
drmtest_srcs  = [ 'drmtest.c', 'drmtools.c', 'ttytools.c', 'render.c', 'image.c' ]
fbinfo_srcs   = [ 'fbinfo.c', 'fbtools.c' ]
fbtest_srcs   = [ 'fbtest.c', 'fbtools.c', 'ttytools.c', 'render.c', 'image.c' ]
prime_srcs    = [ 'prime.c' ]
viotest_srcs  = [ 'virtiotest.c', 'drmtools.c', 'ttytools.c', 'render.c' ]
gtktest_srcs  = [ 'gtktest.c', 'render.c', 'image.c' ]

drminfo_deps  = [ libdrm_dep, cairo_dep, pixman_dep ]
drmtest_deps  = [ libdrm_dep, gbm_dep, epoxy_dep,
                  cairo_dep, pixman_dep, jpeg_dep ]
fbinfo_deps   = [ cairo_dep ]
fbtest_deps   = [ cairo_dep, pixman_dep, jpeg_dep ]
prime_deps    = [ libdrm_dep, gbm_dep ]
viotest_deps  = [ libdrm_dep, gbm_dep, epoxy_dep,
                  cairo_dep, pixman_dep, jpeg_dep ]
gtktest_deps  = [ gtk3_dep,
                  cairo_dep, pixman_dep, jpeg_dep ]

executable('drminfo',
           sources      : drminfo_srcs,
           dependencies : drminfo_deps,
           install      : true)
executable('drmtest',
           sources      : drmtest_srcs,
           dependencies : drmtest_deps,
           install      : true)
executable('fbinfo',
           sources      : fbinfo_srcs,
           dependencies : fbinfo_deps,
           install      : true)
executable('fbtest',
           sources      : fbtest_srcs,
           dependencies : fbtest_deps,
           install      : true)
executable('prime',
           sources      : prime_srcs,
           dependencies : prime_deps,
           install      : true)
executable('virtiotest',
           sources      : viotest_srcs,
           dependencies : viotest_deps,
           install      : true)
executable('gtktest',
           sources      : gtktest_srcs,
           dependencies : gtktest_deps,
           install      : false)

install_man('drminfo.1')
install_man('drmtest.1')
