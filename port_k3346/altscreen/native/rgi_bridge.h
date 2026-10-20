#ifndef K2161_RGI_BRIDGE_H
#define K2161_RGI_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

/* Returns 1 when ready, 0 on failure. */
int k2161_rgi_bridge_init(void);

/*
 * Feed one COMPLETE iAP2 control frame beginning with 40 40.
 *
 * Returns:
 *   1 = accepted RGI frame
 *   0 = ignored/non-RGI
 *  <0 = bridge/parser/bus failure
 */
int k2161_rgi_bridge_feed(const uint8_t *frame, size_t len);

void k2161_rgi_bridge_shutdown(void);

#endif
