/*
 * some generic framebuffer device stuff
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/kd.h>
#include <linux/vt.h>
#include <linux/fb.h>

#include <cairo.h>

#include "fbtools.h"
#include "logind.h"

/* -------------------------------------------------------------------- */
/* internal variables                                                   */

struct fb_fix_screeninfo         fb_fix;
struct fb_var_screeninfo         fb_var;
unsigned char                    *fb_mem;
int			         fb_mem_offset = 0;
cairo_format_t                   fb_format = CAIRO_FORMAT_INVALID;

static int                       fb;
static int                       kd_mode;
static bool                      kd_mode_restore = false;

static struct termios            term;
static struct fb_var_screeninfo  fb_ovar;
static unsigned short            ored[256], ogreen[256], oblue[256];
static struct fb_cmap            ocmap = { 0, 256, ored, ogreen, oblue };

static unsigned short p_red[256], p_green[256], p_blue[256];
static struct fb_cmap p_cmap = { 0, 256, p_red, p_green, p_blue };

/* -------------------------------------------------------------------- */
/* palette handling                                                     */

static unsigned short color_scale(int n, int max)
{
    int ret = 65535.0 * (float)n/(max);
    if (ret > 65535) ret = 65535;
    if (ret <     0) ret =     0;
    return ret;
}

static void fb_linear_palette(int r, int g, int b)
{
    int i, size;

    size = 256 >> (8 - r);
    for (i = 0; i < size; i++)
        p_red[i] = color_scale(i,size);
    p_cmap.len = size;

    size = 256 >> (8 - g);
    for (i = 0; i < size; i++)
        p_green[i] = color_scale(i,size);
    if (p_cmap.len < size)
	p_cmap.len = size;

    size = 256 >> (8 - b);
    for (i = 0; i < size; i++)
	p_blue[i] = color_scale(i,size);
    if (p_cmap.len < size)
	p_cmap.len = size;
}

static void fb_set_palette(void)
{
    if (fb_fix.visual != FB_VISUAL_DIRECTCOLOR && fb_var.bits_per_pixel != 8)
	return;
    if (-1 == ioctl(fb,FBIOPUTCMAP,&p_cmap)) {
	perror("ioctl FBIOPUTCMAP");
	exit(1);
    }
}

/* -------------------------------------------------------------------- */
/* initialisation & cleanup                                             */

static void
fb_memset (void *addr, int c, size_t len)
{
#if 1 /* defined(__powerpc__) */
    unsigned int i, *p;

    i = (c & 0xff) << 8;
    i |= i << 16;
    len >>= 2;
    for (p = addr; len--; p++)
	*p = i;
#else
    memset(addr, c, len);
#endif
}

void fb_fini(void)
{
    /* restore console */
    if (-1 == ioctl(fb, FBIOPUT_VSCREENINFO, &fb_ovar))
	perror("ioctl FBIOPUT_VSCREENINFO");
    if (-1 == ioctl(fb, FBIOGET_FSCREENINFO, &fb_fix))
	perror("ioctl FBIOGET_FSCREENINFO");
    if (fb_ovar.bits_per_pixel == 8 ||
	fb_fix.visual == FB_VISUAL_DIRECTCOLOR) {
	if (-1 == ioctl(fb, FBIOPUTCMAP, &ocmap))
	    perror("ioctl FBIOPUTCMAP");
    }
    close(fb);

    if (kd_mode_restore) {
        if (-1 == ioctl(STDIN_FILENO, KDSETMODE, kd_mode))
            perror("ioctl KDSETMODE");
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

/* -------------------------------------------------------------------- */

void fb_query(int cardno)
{
    char device[64];
    int err;

    snprintf(device, sizeof(device), "/dev/fb%d", cardno);
    if (-1 == (fb = open(device,O_RDWR | O_CLOEXEC))) {
        err = errno;
        if (-1 == (fb = logind_open(device))) {
            fprintf(stderr,"open %s: %s\n",device,strerror(err));
            exit(1);
        }
    }
    if (-1 == ioctl(fb,FBIOGET_FSCREENINFO,&fb_fix)) {
	perror("ioctl FBIOGET_FSCREENINFO");
	exit(1);
    }
    if (-1 == ioctl(fb,FBIOGET_VSCREENINFO,&fb_var)) {
	perror("ioctl FBIOGET_VSCREENINFO");
	exit(1);
    }
}

void fb_init(int cardno)
{
    unsigned long page_mask;
    char device[64];
    int err;

    snprintf(device, sizeof(device), "/dev/fb%d", cardno);

    if (-1 == ioctl(STDIN_FILENO, KDGETMODE, &kd_mode)) {
	perror("ioctl KDGETMODE");
    } else {
        kd_mode_restore = true;
    }

    /* get current settings (which we have to restore) */
    if (-1 == (fb = open(device,O_RDWR | O_CLOEXEC))) {
        err = errno;
        if (-1 == (fb = logind_open(device))) {
            fprintf(stderr,"open %s: %s\n",device,strerror(err));
            exit(1);
        }
    }
    if (-1 == ioctl(fb,FBIOGET_VSCREENINFO,&fb_ovar)) {
	perror("ioctl FBIOGET_VSCREENINFO");
	exit(1);
    }
    if (-1 == ioctl(fb,FBIOGET_FSCREENINFO,&fb_fix)) {
	perror("ioctl FBIOGET_FSCREENINFO");
	exit(1);
    }
    if (-1 == ioctl(fb,FBIOGET_VSCREENINFO,&fb_var)) {
	perror("ioctl FBIOGET_VSCREENINFO");
	exit(1);
    }
    if (fb_ovar.bits_per_pixel == 8 ||
	fb_fix.visual == FB_VISUAL_DIRECTCOLOR) {
	if (-1 == ioctl(fb,FBIOGETCMAP,&ocmap)) {
	    perror("ioctl FBIOGETCMAP");
	    exit(1);
	}
    }
    tcgetattr(STDIN_FILENO, &term);

    /* checks & initialisation */
    if (kd_mode_restore) {
        if (-1 == ioctl(STDIN_FILENO, KDSETMODE, KD_GRAPHICS))
            perror("ioctl KDSETMODE");
    }
    if (-1 == ioctl(fb,FBIOGET_FSCREENINFO,&fb_fix)) {
	perror("ioctl FBIOGET_FSCREENINFO");
	exit(1);
    }
    if (fb_fix.type != FB_TYPE_PACKED_PIXELS) {
	fprintf(stderr,"can handle only packed pixel frame buffers\n");
	goto err;
    }
    page_mask = getpagesize()-1;
    fb_mem_offset = (unsigned long)(fb_fix.smem_start) & page_mask;
    fb_mem = mmap(NULL,fb_fix.smem_len+fb_mem_offset,
		  PROT_READ|PROT_WRITE,MAP_SHARED,fb,0);
    if (-1L == (long)fb_mem) {
	perror("mmap");
	goto err;
    }
    /* move viewport to upper left corner */
    if (fb_var.xoffset != 0 || fb_var.yoffset != 0) {
	fb_var.xoffset = 0;
	fb_var.yoffset = 0;
	if (-1 == ioctl(fb,FBIOPAN_DISPLAY,&fb_var)) {
	    perror("ioctl FBIOPAN_DISPLAY");
	    goto err;
	}
    }

    /* cls */
    fb_memset(fb_mem+fb_mem_offset, 0, fb_fix.line_length * fb_var.yres);

    /* init palette */
    switch (fb_var.bits_per_pixel) {
    case 16:
        fb_format = CAIRO_FORMAT_RGB16_565;
        fb_linear_palette(5,6,5);
	break;
    case 32:
        fb_format = CAIRO_FORMAT_RGB24;
        fb_linear_palette(8,8,8);
	break;
    default:
        fprintf(stderr, "unsupported framebuffer format (%d bpp)\n",
                fb_var.bits_per_pixel);
        goto err;

    }
    fb_set_palette();
    return;

 err:
    fb_fini();
    exit(1);
}
