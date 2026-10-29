/* K3346 AAC-LC backend for the reference codec table. No Cinemo globals.
 * Decoder instances are owned and serialized by the receiver's codec worker. */
#include "apac_bridge.h"
#include "aacdecoder_lib.h"
#include "resample_44k_to_48k.h"
#include <stdlib.h>
#include <string.h>

struct spa_apac_decoder {
    HANDLE_AACDECODER handle;
    unsigned char asc[8];
    unsigned asc_size;
    int failed;
    int is44;                 /* index 22: AAC-LC/44.1k, resampled to 48k inside the bridge */
    struct rs44_state rs;
    int16_t raw[2048];
};

/* Descend only inside each descriptor's declared payload. ES optional fields
 * have independent bounds; reject unterminated lengths and duplicate ASC. */
static int find_asc(const unsigned char *p,size_t n,unsigned depth,
                    unsigned char *asc,unsigned *length)
{
    if(depth>4)return 0;
    while(n){
        unsigned tag=*p++,size=0,count=0,b;--n;
        do{
            if(!n||count++==4)return 0;
            b=*p++;--n;size=(size<<7)|(b&127);
        }while(b&128);
        if(size>n)return 0;
        const unsigned char *q=p;size_t payload=size,skip=0;
        if(tag==5){
            if(*length||size!=2)return 0;
            memcpy(asc,p,size);*length=size;
        }else if(tag==3){
            if(payload<3)return 0;
            unsigned flags=q[2];skip=3;
            if(flags&128)skip+=2;
            if(skip>payload)return 0;
            if(flags&64){if(skip==payload)return 0;unsigned url=q[skip++];skip+=url;}
            if(flags&32)skip+=2;
            if(skip>payload||!find_asc(q+skip,payload-skip,depth+1,asc,length))return 0;
        }else if(tag==4){
            if(payload<13||q[0]!=0x40)return 0;
            if(!find_asc(q+13,payload-13,depth+1,asc,length))return 0;
        }
        p+=size;n-=size;
    }
    return 1;
}

static int restart(spa_apac_decoder_t *d)
{
    if(d->handle)aacDecoder_Close(d->handle);
    d->handle=aacDecoder_Open(TT_MP4_RAW,1);d->failed=1;
    if(!d->handle)return SPA_APAC_ERR_NO_MEMORY;
    unsigned char *p=d->asc;unsigned n=d->asc_size;
    if(aacDecoder_ConfigRaw(d->handle,&p,&n)!=AAC_DEC_OK)return SPA_APAC_ERR_INIT;
    /* FDK otherwise adds an automatic 15ms peak-limiter lookahead. Timing
     * and optional limiting belong to the receiver/SoundCheck renderer. */
    if(aacDecoder_SetParam(d->handle,AAC_PCM_LIMITER_ENABLE,0)!=AAC_DEC_OK)return SPA_APAC_ERR_INIT;
    d->failed=0;return SPA_APAC_OK;
}

static void destroy(spa_apac_decoder_t *d)
{
    if(!d)return;
    if(d->handle)aacDecoder_Close(d->handle);
    free(d);
}

static int create(const uint8_t *cookie,size_t bytes,spa_apac_decoder_t **out,
                    uint32_t *channels,uint32_t *frames,uint32_t *rate)
{
    if(!out)return SPA_APAC_ERR_ARGS;
    *out=0;
    if(!cookie||!bytes||bytes>4096||!channels||!frames||!rate)return SPA_APAC_ERR_ARGS;
    unsigned char asc[8];unsigned n=0;
    if(!find_asc(cookie,bytes,0,asc,&n)||n!=2)return SPA_APAC_ERR_COOKIE;
    /* Index 23 (AAC-LC/48k/stereo) or the index 22 fallback (AAC-LC/44.1k/stereo, resampled
     * to 48k here before the ring).  Any other geometry must select another backend. */
    int is44=asc[0]==0x12&&asc[1]==0x10;
    if(!is44&&(asc[0]!=0x11||asc[1]!=0x90))return SPA_APAC_ERR_CONFIG;
    spa_apac_decoder_t *d=calloc(1,sizeof(*d));if(!d)return SPA_APAC_ERR_NO_MEMORY;
    memcpy(d->asc,asc,n);d->asc_size=n;d->is44=is44;rs44_reset(&d->rs);
    int r=restart(d);if(r){destroy(d);return r;}
    *out=d;*channels=2;*frames=1024;*rate=48000;return SPA_APAC_OK;
}

static void reset(spa_apac_decoder_t *d){if(d){rs44_reset(&d->rs);(void)restart(d);}}

static int decode(spa_apac_decoder_t *d,const uint8_t *packet,size_t bytes,
                    int16_t *out,size_t capacity,uint32_t *frames)
{
    if(!frames)return SPA_APAC_ERR_ARGS;
    *frames=0;if(!d)return SPA_APAC_ERR_ARGS;
    if(!bytes){rs44_reset(&d->rs);return restart(d);}
    if(!packet||!out||bytes>8192)return SPA_APAC_ERR_ARGS;
    if(capacity<(d->is44?2*RS_MAX_OUT:2048))return SPA_APAC_ERR_OUTPUT_SPACE;
    if(d->failed)return SPA_APAC_ERR_DECODE;
    unsigned char *p=(unsigned char *)packet;unsigned n=(unsigned)bytes,valid=n;
    int16_t *target=d->is44?d->raw:out;
    if(aacDecoder_Fill(d->handle,&p,&n,&valid)!=AAC_DEC_OK||valid){d->failed=1;return SPA_APAC_ERR_DECODE;}
    if(aacDecoder_DecodeFrame(d->handle,target,2048,0)!=AAC_DEC_OK){d->failed=1;return SPA_APAC_ERR_DECODE;}
    const CStreamInfo *info=aacDecoder_GetStreamInfo(d->handle);
    if(!info||info->sampleRate!=(d->is44?44100:48000)||info->frameSize!=1024||info->numChannels!=2){d->failed=1;return SPA_APAC_ERR_CHANNELS;}
    if(d->is44){*frames=(uint32_t)rs44_process(&d->rs,d->raw,1024,out);return SPA_APAC_OK;}
    *frames=1024;return SPA_APAC_OK;
}

static int get_info(const spa_apac_decoder_t *d,spa_apac_stream_info_t *info)
{
    if(!d||!info)return SPA_APAC_ERR_ARGS;
    info->channels=2;info->frameLength=1024;info->sampleRate=48000;info->channelLayoutTag=0;
    return SPA_APAC_OK;
}

const spa_apac_codec_api_t *spa_aac_codec_api(void)
{
    static const spa_apac_codec_api_t api={sizeof(api),SPA_APAC_CODEC_ABI_VERSION,
        create,decode,reset,destroy,get_info};
    return &api;
}
