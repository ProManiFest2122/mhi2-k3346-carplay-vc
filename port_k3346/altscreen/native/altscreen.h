#ifndef ALTSCREEN_H
#define ALTSCREEN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AIRPLAY_STREAM_TYPE_ALT_SCREEN 111
#define ALTSCREEN_UUID "E0CB6FB0-0000-0000-0000-0000C0FFEE58"
#define ALTSCREEN_INITIAL_URL "maps:/car/instrumentcluster"

/* Feature control / AirPlay capability state. */
int altscreen_enabled(void);
int altscreen_info_ready(void);

/* Independent AltScreen session tracking. */
int altscreen_register_session(void *screen_session);
void altscreen_unregister_session(void *screen_session);
int altscreen_is_session(void *screen_session);
int altscreen_start_session(void *screen_session, void *delegate_context);
int altscreen_session_scope_active(void);

/* State captured from the stock primary screen path. */
void *altscreen_main_delegate_context(void);
int altscreen_time_sync_ready(void);
int altscreen_apply_main_time_sync(void *screen_session);
int altscreen_aes_ready(void);
int altscreen_derive_aes(uint64_t stream_connection_id,
                         uint8_t out_key[16],
                         uint8_t out_iv[16]);

/* Request a fresh secondary-display IDR through the live AirPlay session. */
int altscreen_request_fresh_keyframe(void);

/* Decoder/output diagnostics. */
int altscreen_output_ready(void);
unsigned int altscreen_output_patch_count(void);
void altscreen_v13_note_geometry_ready(void *stream);

#ifdef __cplusplus
}
#endif

#endif /* ALTSCREEN_H */
