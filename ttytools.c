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

#include "ttytools.h"

static struct termios  saved_attributes;
static int             saved_fl;

void tty_raw(void)
{
    struct termios tattr;

    fcntl(STDIN_FILENO, F_GETFL, &saved_fl);
    tcgetattr (0, &saved_attributes);

    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    memcpy(&tattr,&saved_attributes,sizeof(struct termios));
    tattr.c_lflag &= ~(ICANON|ECHO);
    tattr.c_cc[VMIN] = 1;
    tattr.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);
}

void tty_restore(void)
{
    fcntl(STDIN_FILENO, F_SETFL, saved_fl);
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_attributes);
}

int kbd_wait(int timeout)
{
    struct timeval limit;
    fd_set set;
    int rc;

    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    limit.tv_sec = timeout;
    limit.tv_usec = 0;
    rc = select(STDIN_FILENO + 1, &set, NULL, NULL,
                timeout ? &limit : NULL);
    return rc;
}
