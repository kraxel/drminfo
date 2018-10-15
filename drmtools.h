struct fbformat {
    const char            name[8];
    const char            fields[16];
    const char            bits[16];
    uint32_t              bpp;      /*  bytes per pixel         */
    uint32_t              depth;    /*  legacy        (ADDFB)   */
    uint32_t              fourcc;   /*  DRM_FORMAT_*  (ADDFB2)  */
    cairo_format_t        cairo;    /*  CAIRO_FORMAT_*          */
    pixman_format_code_t  pixman;   /* PIXMAN_*                 */
};

extern const struct fbformat fmts[];
extern const uint32_t fmtcnt;

/* ------------------------------------------------------------------ */

const char *drm_connector_type_name(int nr);
const char *drm_connector_mode_name(int nr);
const char *drm_encoder_type_name(int nr);
void drm_conn_name(drmModeConnector *conn, char *dest, int dlen);
bool drm_probe_format(int fd, const struct fbformat *fmt);
void drm_print_format(FILE *fp, const struct fbformat *fmt,
                      int indent, bool libs);
void drm_print_format_hdr(FILE *fp, int indent, bool libs);

/* ------------------------------------------------------------------ */

extern int fd;
extern uint32_t fb_id;
extern drmModeConnector *conn;
extern drmModeModeInfo *mode;

void drm_init_dev(int devnr, const char *output,
                  const char *modename, bool need_dumb);
void drm_fini_dev(void);
void drm_show_fb(void);
