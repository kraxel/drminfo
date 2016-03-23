CC	?= gcc
CFLAGS	?= -Os -g -std=c99
CFLAGS	+= -Wall

TARGETS	:= drminfo drmtest

drminfo : CFLAGS += $(shell pkg-config --cflags libdrm)
drminfo : LDLIBS += $(shell pkg-config --libs libdrm)

drmtest : CFLAGS += $(shell pkg-config --cflags libdrm gbm epoxy)
drmtest : LDLIBS += $(shell pkg-config --libs libdrm gbm epoxy)

all: $(TARGETS)

clean:
	rm -f $(TARGETS)
	rm -f *~

drminfo: drminfo.o drmtools.o
drmtest: drmtest.o drmtools.o
