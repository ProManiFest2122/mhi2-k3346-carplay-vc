/* Fail-closed gate: with MHI2_NV_SIX_CHANNEL_QUALIFIED=0 the hook must not advertise 5.1 or enable spatial/realtime PCM even when
 * the environment asks for it; with =1 (control) it does. Host only. */
#define ENABLE_LOGGING 1
#define ENABLE_STATE_TRACE 0
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "../artifacts/full-audio/port/audio_hook_k3346_nv.c"
void log_write(log_level_t level,const char *module,const char *fmt,...){(void)level;(void)module;(void)fmt;}
bool log_verbose(void){return false;}
hook_result_t hook_inject_message(uint16_t m,const uint8_t *p,size_t n){(void)m;(void)p;(void)n;return HOOK_ERR_BUSY;}
const spa_apac_codec_api_t *spa_apac_runtime_load(const char **path){(void)path;return 0;}
int main(void){
    assert(!setenv("AUDIO_SPATIAL","1",1));assert(!setenv("AUDIO_REALTIME_UPMIX","on",1));assert(!setenv("AUDIO_STEREO_UPMIX","on",1));
    assert(!setenv("AUDIO_SPATIAL_ATMOS","1",1));
    int64_t caps=AudioSessionGetSupportedOutputFormats(100);
    unsigned bit,channels;spa_tune(&bit,&channels);
    int advertised=(caps>>bit)&1;
    printf("qualified=%d spa_enabled=%d buffered=%d advertised51=%d\n",MHI2_NV_SIX_CHANNEL_QUALIFIED,spa_enabled(),spa_audio_config()->buffered,advertised);
#if MHI2_NV_SIX_CHANNEL_QUALIFIED
    assert(spa_enabled()&&advertised);
#else
    assert(!spa_enabled()&&!spa_audio_config()->buffered&&!advertised&&!spa_audio_config()->atmos);
#endif
    puts("NV INERT GATE PASS");return 0;
}
