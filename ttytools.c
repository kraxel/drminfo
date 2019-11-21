#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <fcntl.h>
#include <time.h>
#include <termios.h>

#include <sys/time.h>

#include <libudev.h>
#include <libinput.h>

#include "ttytools.h"
#include "logind.h"

static struct termios  saved_attributes;
static int             saved_fl;

static struct libinput *ctx;

const struct libinput_interface libinput_if = {
    .open_restricted  = logind_open,
    .close_restricted = logind_close,
};

void tty_raw(void)
{
    struct termios tattr;
    struct udev *udev;

    if (have_logind()) {
        udev = udev_new();
        ctx = libinput_udev_create_context(&libinput_if, NULL, udev);
        libinput_udev_assign_seat(ctx, getenv("XDG_SEAT"));
    } else {
        fcntl(STDIN_FILENO, F_GETFL, &saved_fl);
        tcgetattr (0, &saved_attributes);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
        memcpy(&tattr,&saved_attributes,sizeof(struct termios));
        tattr.c_lflag &= ~(ICANON|ECHO);
        tattr.c_cc[VMIN] = 1;
        tattr.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);
    }
}

void tty_restore(void)
{
    if (have_logind()) {
        libinput_unref(ctx);
    } else {
        fcntl(STDIN_FILENO, F_SETFL, saved_fl);
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_attributes);
    }
}

int kbd_wait(int timeout)
{
    struct timeval limit;
    fd_set set;
    int fd, rc;

    if (have_logind()) {
        fd = libinput_get_fd(ctx);
    } else {
        fd = STDIN_FILENO;
    }

    FD_ZERO(&set);
    FD_SET(fd, &set);
    limit.tv_sec = timeout;
    limit.tv_usec = 0;
    rc = select(fd + 1, &set, NULL, NULL,
                timeout ? &limit : NULL);
    return rc;
}

void kbd_read(void)
{
    char buf[32];

    if (have_logind()) {
        /* nothing */
    } else {
        read(0, buf, sizeof(buf));
    }
}
