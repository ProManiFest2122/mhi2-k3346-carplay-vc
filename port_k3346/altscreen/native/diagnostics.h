#ifndef MIB2HIGH_RGI_DIAGNOSTICS_H
#define MIB2HIGH_RGI_DIAGNOSTICS_H

#include <stddef.h>
#include <stdint.h>

/* K3346 Etap 4: overridable so the mm-ipod-only RGI build (build_rgi_ipod.sh)
 * does not truncate the dio_manager copy's file. Default unchanged. */
#ifndef MIB2HIGH_RGI_DIAGNOSTICS_PATH
#define MIB2HIGH_RGI_DIAGNOSTICS_PATH "/dev/shmem/mib2high-carplay-rgi-native.log"
#endif

void rgi_diag_reset_file(void);
void rgi_diag_log(const char *layer, const char *fmt, ...);
uint32_t rgi_diag_session_get(void);
uint32_t rgi_diag_session_bump(const char *reason);
uint32_t rgi_diag_rx_begin(uint16_t msgid, size_t len);
void rgi_diag_rx_end(uint32_t seq, int bridge_rc);
uint32_t rgi_diag_current_rx_seq(void);
uint16_t rgi_diag_current_msgid(void);

#endif
