struct fbformat {
    const char            name[8];
    const char            fields[16];
    const char            bits[16];
    uint32_t              bpp;      /*  bytes per pixel         */
    uint32_t              depth;    /*  legacy        (ADDFB)   */
    uint32_t              fourcc;   /*  DRM_FORMAT_*  (ADDFB2)  */
    cairo_format_t        cairo;    /*  CAIRO_FORMAT_*          */
    pixman_format_code_t  pixman;   /*  PIXMAN_*                */
    uint32_t              virtio;   /*  VIRTIO_GPU_FORMAT_*     */
};

extern const struct fbformat fmts[];
extern const uint32_t fmtcnt;

/* ------------------------------------------------------------------ */

const char *drm_connector_type_name(int nr);
const char *drm_connector_mode_name(int nr);
const char *drm_encoder_type_name(int nr);
void drm_conn_name(drmModeConnector *conn, char *dest, int dlen);

uint64_t drm_get_property_value(int fd, uint32_t id, uint32_t objtype,
                                const char *name);
bool drm_probe_format_primary(const struct fbformat *fmt);
bool drm_probe_format_cursor(const struct fbformat *fmt);
void drm_plane_init(int fd);

bool drm_probe_format_fb(int fd, const struct fbformat *fmt);
void drm_print_format(FILE *fp, const struct fbformat *fmt,
                      int indent, bool libs, bool virtio);
void drm_print_format_hdr(FILE *fp, int indent,
                          bool libs, bool virtio);

/* ------------------------------------------------------------------ */

extern int drm_fd;
extern uint32_t fb_id;
extern drmModeConnector *drm_conn;
extern drmModeModeInfo *drm_mode;
extern drmModeEncoder *drm_enc;
extern drmVersion *version;

void drm_init_dev(int devnr, const char *output,
                  const char *modename, bool need_dumb,
                  int lease_fd);
int drm_init_vgem(void);
void drm_fini_dev(void);
void drm_show_fb(void);

/* drmtools-egl.c */
int drm_setup_egl(void);
void drm_egl_flush_display(void);
