const char *drm_connector_type_name(int nr);
const char *drm_connector_mode_name(int nr);
const char *drm_encoder_type_name(int nr);
void drm_conn_name(drmModeConnector *conn, char *dest, int dlen);
bool drm_probe_format(int fd, uint32_t bpp, uint32_t depth, uint32_t fourcc, bool print);
