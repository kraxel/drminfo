#include <linux/fb.h>

extern struct fb_fix_screeninfo fb_fix;
extern struct fb_var_screeninfo fb_var;
extern unsigned char            *fb_mem;
extern int		        fb_mem_offset;
extern cairo_format_t           fb_format;

void fb_init(int devnr);
void fb_fini(void);
