CC	?= gcc
CFLAGS	?= -Os -g -std=c99
CFLAGS	+= -Wall

TARGETS	:= drminfo drmtest gtktest

drminfo : CFLAGS += $(shell pkg-config --cflags libdrm)
drminfo : LDLIBS += $(shell pkg-config --libs libdrm)

drmtest : CFLAGS += $(shell pkg-config --cflags libdrm gbm epoxy cairo cairo-gl)
drmtest : LDLIBS += $(shell pkg-config --libs libdrm gbm epoxy cairo cairo-gl)

gtktest : CFLAGS += $(shell pkg-config --cflags gtk+-3.0 cairo)
gtktest : LDLIBS += $(shell pkg-config --libs gtk+-3.0 cairo)

all: $(TARGETS)

clean:
	rm -f $(TARGETS)
	rm -f *~ *.o

drminfo: drminfo.o drmtools.o
drmtest: drmtest.o drmtools.o render.o
gtktest: gtktest.o render.o
