/* Emulator-only dynamic AAC-LC decoder canary, independent of Cinemo. */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "apac_bridge.h"
extern const unsigned char aac_fixture[],aac_fixture_end[];
static unsigned rd(const unsigned char *p){return (unsigned)p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);}
static void report(const char *key,unsigned a,unsigned b){char s[160];int n=snprintf(s,sizeof(s),"AAC_NATIVE %s=%u detail=%u\n",key,a,b);write(1,s,n);}
static short pcm[2048];
void _start(void){
 void *lib=dlopen("/lib/libmhi2_fdk.so",RTLD_NOW);if(!lib){report("loader_error",1,0);exit(1);}
 void *(*open_decoder)(int,unsigned)=(void *)dlsym(lib,"aacDecoder_Open");
 int (*config)(void *,unsigned char **,unsigned *)=(void *)dlsym(lib,"aacDecoder_ConfigRaw");
 int (*fill)(void *,unsigned char **,unsigned *,unsigned *)=(void *)dlsym(lib,"aacDecoder_Fill");
 int (*decode)(void *,short *,int,unsigned)=(void *)dlsym(lib,"aacDecoder_DecodeFrame");
 const int *(*info)(void *)=(void *)dlsym(lib,"aacDecoder_GetStreamInfo");
 void (*close_decoder)(void *)=(void *)dlsym(lib,"aacDecoder_Close");
 int (*set_param)(void *,int,int)=(void *)dlsym(lib,"aacDecoder_SetParam");
 if(!open_decoder||!config||!fill||!decode||!info||!close_decoder||!set_param)exit(2);
 unsigned packets=rd(aac_fixture+8),previous=0;
 for(unsigned pass=0;pass<2;pass++){
  void *d=open_decoder(0,1);if(!d)exit(3);
  unsigned char asc[2]={0x11,0x90},*p=asc;unsigned n=2;
  if(config(d,&p,&n))exit(4);
  if(set_param(d,4,0))exit(16);
  if(!pass)report("limiter_disabled",1,0);
  const unsigned char *at=aac_fixture+12;unsigned hash=2166136261u,peaks[2]={0,0};
  for(unsigned packet=0;packet<packets;packet++){
   if(at+4>aac_fixture_end)exit(5);n=rd(at);at+=4;
   if(!n||n>(unsigned)(aac_fixture_end-at))exit(6);
   unsigned valid=n;p=(unsigned char *)at;at+=n;
   if(fill(d,&p,&n,&valid)||valid)exit(7);
   if(decode(d,pcm,2048,0))exit(8);
   const int *geometry=info(d);if(!geometry||geometry[0]!=48000||geometry[1]!=1024||geometry[2]!=2)exit(9);
   for(unsigned i=0;i<2048;i++){
    int sample=pcm[i];unsigned magnitude=sample<0?(unsigned)-sample:(unsigned)sample;
    if(magnitude>peaks[i&1])peaks[i&1]=magnitude;
    hash=(hash^(unsigned char)sample)*16777619u;hash=(hash^(unsigned char)((unsigned short)sample>>8))*16777619u;
   }
  }
  close_decoder(d);report("pass",pass,hash);report("left_peak",peaks[0],peaks[1]);
  if(pass&&hash!=previous)exit(10);previous=hash;
 }
 void *bridge=dlopen("/lib/libmhi2_aac.so",RTLD_NOW);if(!bridge){report("bridge_loader_error",1,0);exit(11);}
 const spa_apac_codec_api_t *(*get_api)(void)=(void *)dlsym(bridge,"spa_aac_codec_api");
 const spa_apac_codec_api_t *api=get_api?get_api():0;
 if(!api||api->abiVersion!=1||api->structSize!=sizeof(*api))exit(12);
 const unsigned char cookie[]={3,0x80,0x80,0x80,0x22,0,0,0,4,0x80,0x80,0x80,0x14,0x40,0x14,0,0x18,0,0,1,0xf4,0,0,1,0xf4,0,5,0x80,0x80,0x80,2,0x11,0x90,6,0x80,0x80,0x80,1,2};
 spa_apac_decoder_t *decoder=0;uint32_t channels=0,frames=0,rate=0;
 if(api->create(cookie,sizeof(cookie),&decoder,&channels,&frames,&rate)||channels!=2||frames!=1024||rate!=48000)exit(13);
 for(unsigned pass=0;pass<2;pass++){
  if(pass)api->reset(decoder);
  const unsigned char *at=aac_fixture+12;unsigned hash=2166136261u;
  for(unsigned packet=0;packet<packets;packet++){
   unsigned n=rd(at);at+=4;
   if(api->decodeS16(decoder,at,n,pcm,2048,&frames)||frames!=1024)exit(14);at+=n;
   for(unsigned i=0;i<2048;i++){
    unsigned short sample=(unsigned short)pcm[i];
    hash=(hash^(unsigned char)sample)*16777619u;hash=(hash^(unsigned char)(sample>>8))*16777619u;
   }
  }
  report("bridge_pass",pass,hash);if(hash!=previous)exit(15);
 }
 api->destroy(decoder);dlclose(bridge);dlclose(lib);report("PASS",packets,packets*1024);exit(0);
}
