MESON	:= $(shell which meson 2>&1)

ifneq ($(MESON),)

HOST	:= $(shell hostname -s)
BDIR	:= build-meson-$(HOST)

build: $(BDIR)/build.ninja
	ninja-build -C $(BDIR)

install: build
	ninja-build -C $(BDIR) install

clean:
	rm -rf $(BDIR)

$(BDIR)/build.ninja:
	$(MESON) $(BDIR)

else

CC	?= gcc
CFLAGS	?= -Os -g -std=c99
CFLAGS	+= -Wall

TARGETS	:= drminfo drmtest gtktest

drminfo : CFLAGS += $(shell pkg-config --cflags libdrm cairo pixman-1)
drminfo : LDLIBS += $(shell pkg-config --libs libdrm cairo pixman-1)

drmtest : CFLAGS += $(shell pkg-config --cflags libdrm gbm epoxy cairo cairo-gl pixman-1)
drmtest : LDLIBS += $(shell pkg-config --libs libdrm gbm epoxy cairo cairo-gl pixman-1)
drmtest : LDLIBS += -ljpeg

gtktest : CFLAGS += $(shell pkg-config --cflags gtk+-3.0 cairo pixman-1)
gtktest : LDLIBS += $(shell pkg-config --libs gtk+-3.0 cairo pixman-1)
gtktest : LDLIBS += -ljpeg

all: $(TARGETS)

clean:
	rm -f $(TARGETS)
	rm -f *~ *.o

drminfo: drminfo.o drmtools.o
drmtest: drmtest.o drmtools.o render.o image.o
gtktest: gtktest.o render.o image.o

endif
