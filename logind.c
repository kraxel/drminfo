#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <termios.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>

#include "config.h"
#ifdef HAVE_SYSTEMD
# include <systemd/sd-bus.h>
#endif

#include "logind.h"

#ifdef HAVE_SYSTEMD

/* ---------------------------------------------------------------------- */

static int logind_debug = 0;
static sd_bus *logind_dbus = NULL;

/* ---------------------------------------------------------------------- */

static int logind_take_control(void)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    int r;

    if (!logind_dbus)
        return -1;

    if (logind_debug)
        fprintf(stderr, "call   : TakeControl()\n");
    r = sd_bus_call_method(logind_dbus,
                           "org.freedesktop.login1",
                           "/org/freedesktop/login1/session/self",
                           "org.freedesktop.login1.Session",
                           "TakeControl",
                           &error,
                           &m,
                           "b",
                           false);
    if (r < 0) {
        fprintf(stderr, "error  : TakeControl failed: %s\n",
                error.message);
        sd_bus_error_free(&error);
    }
    sd_bus_message_unref(m);

    return r;
}

static int logind_release_control(void)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    int r;

    if (!logind_dbus)
        return -1;

    if (logind_debug)
        fprintf(stderr, "call   : ReleaseControl()\n");
    r = sd_bus_call_method(logind_dbus,
                           "org.freedesktop.login1",
                           "/org/freedesktop/login1/session/self",
                           "org.freedesktop.login1.Session",
                           "ReleaseControl",
                           &error,
                           &m,
                           "");
    if (r < 0) {
        fprintf(stderr, "error  : ReleaseControl failed: %s\n",
                error.message);
        sd_bus_error_free(&error);
    }
    sd_bus_message_unref(m);

    return r;
}

/* ---------------------------------------------------------------------- */

void logind_init(void)
{
    const char *session_id, *seat;
    int r;

    if (logind_dbus)
        return;

    seat = getenv("XDG_SEAT");
    session_id = getenv("XDG_SESSION_ID");
    if (!seat || !session_id)
        return;

    r = sd_bus_open_system(&logind_dbus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        return;
    }

    r = logind_take_control();
    if (r < 0)
        goto err;

    return;

err:
    sd_bus_unref(logind_dbus);
    logind_dbus = NULL;
}

void logind_fini(void)
{
    if (!logind_dbus)
        return;

    logind_release_control();
    sd_bus_unref(logind_dbus);
    logind_dbus = NULL;
}

int logind_open(const char *path, int flags, void *user_data)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    struct stat st;
    unsigned int maj, min;
    int inactive;
    int handle, fd, r;

    if (!logind_dbus)
        return -1;

    r = stat(path, &st);
    if (r < 0) {
        fprintf(stderr, "stat %s failed: %s\n", path, strerror(errno));
        return -1;
    }

    maj = major(st.st_rdev);
    min = minor(st.st_rdev);
    if (logind_debug)
        fprintf(stderr, "call   : TakeDevice(%d,%d)\n", maj, min);
    r = sd_bus_call_method(logind_dbus,
                           "org.freedesktop.login1",
                           "/org/freedesktop/login1/session/self",
                           "org.freedesktop.login1.Session",
                           "TakeDevice",
                           &error,
                           &m,
                           "uu",
                           maj,
                           min);
    if (r < 0) {
        fprintf(stderr, "error  : TakeDevice failed: %s\n",
                error.message);
        sd_bus_error_free(&error);
        return -1;
    }

    handle = -1;
    inactive = -1;
    r = sd_bus_message_read(m, "hb", &handle, &inactive);
    if (r < 0) {
        fd = -1;
        fprintf(stderr, "error  : Parsing TakeDevice reply failed: %s\n",
                strerror(-r));
    } else {
        fd = fcntl(handle, F_DUPFD_CLOEXEC, 0);
#if 0
        fprintf(stderr, "open %s: got fd %d via logind.\n",
                path, fd);
#endif
    }
    sd_bus_message_unref(m);

    return fd;
}

void logind_close(int fd, void *user_data)
{
    /* FIXME */
}

bool have_logind(void)
{
    return logind_dbus != NULL;
}

#else

/* ---------------------------------------------------------------------- */

void logind_init(void)
{
}

void logind_fini(void)
{
}

int logind_open(const char *path, int flags, void *user_data)
{
    const char *session_id, *seat;

    seat = getenv("XDG_SEAT");
    session_id = getenv("XDG_SESSION_ID");
    if (!seat || !session_id)
        return -1;

    fprintf(stderr, "%s(%s): compiled without logind support.\n",
            __func__, path);
    return -1;
}

void logind_close(int fd, void *user_data)
{
}

bool have_logind(void)
{
    return false;
}

#endif

/* ---------------------------------------------------------------------- */

int device_open(const char *device)
{
    int fd;

    if (have_logind()) {
        fd = logind_open(device, O_RDWR, NULL);
        if (fd < 0) {
            fprintf(stderr,"logind: can't get handle for %s\n", device);
            exit(1);
        }
    } else {
        fd = open(device, O_RDWR | O_CLOEXEC);
        if (fd < 0) {
            fprintf(stderr,"open %s: %s\n", device, strerror(errno));
            exit(1);
        }
    }
    return fd;
}
