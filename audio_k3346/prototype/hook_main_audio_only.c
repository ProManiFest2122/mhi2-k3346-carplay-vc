/* Standalone K3346 audio module: the reference framework seams (airplay_seams.c, http210_seams.c) with exactly one
 * module (audio) and a minimal framework core.  No iAP2/Cinemo/bus: the AirPlay seam entry runs on_init once. */
#include "hook_framework.h"
#include <pthread.h>
extern const hook_module_def_t audio_module_def;
const hook_module_def_t *const hook_module_table[] = { &audio_module_def };
const size_t hook_module_table_count = 1;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;
static void init_once(void)
{
    if (audio_module_def.on_init) audio_module_def.on_init();
}
hook_result_t hook_framework_init(void)
{
    pthread_once(&g_once, init_once);
    return HOOK_OK;
}
size_t hook_framework_module_count(void) { return hook_module_table_count; }
const hook_module_def_t *hook_framework_module_at(size_t i) { return i < hook_module_table_count ? hook_module_table[i] : 0; }
/* No iAP2 transport in this build; the NowPlaying injection used for SCENE track change is unavailable. */
hook_result_t hook_inject_message(uint16_t msgid, const uint8_t *payload, size_t len)
{
    (void)msgid; (void)payload; (void)len;
    return HOOK_ERR_BUSY;
}
