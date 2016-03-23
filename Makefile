CC	?= gcc
CFLAGS	?= -Os -g -std=c99
CFLAGS	+= -Wall

TARGETS	:= drminfo drmtest

CFLAGS += $(shell pkg-config --cflags libdrm)
LDLIBS += $(shell pkg-config --libs libdrm)

all: $(TARGETS)

clean:
	rm -f $(TARGETS)
	rm -f *~

drminfo: drminfo.o drmtools.o
drmtest: drmtest.o drmtools.o
