/* Full reference render_frame -> real codecs -> actual ring -> NVIDIA pump.
 * The native hardware boundary is injected. HTTP/TCP and focus are not tested. */
#ifdef MHI2_PIPELINE_NATIVE
/* Only used by the included, skipped socket mocks; no native socket is opened. */
#define AF_INET 2
#endif
#define main reference_interposer_main
#include "../artifacts/native-nvidia-sink/reference_interposer_with_codec.c"
#undef main
#define main reference_type103_main
#include "../reference-audio-a63d547/tests/apac_type103_test.c"
#undef main
#include "nvidia_receiver_pump.h"
static spa_apac_codec_api_t real_apac_api;
static unsigned pipeline_writes,pipeline_frames;
static unsigned long long pipeline_energy[6];
#ifdef MHI2_PIPELINE_NATIVE
extern int nvaudio_pcm_open(void **,const char *),nvaudio_set_pcm_info(void *,const void *),nvaudio_create_metadata(void *,const void *,unsigned *),nvaudio_pcm_write(void *,const void *,unsigned,const unsigned *,unsigned),nvaudio_pcm_close(void *);
static int pipe_open(void **h,const char *name){return nvaudio_pcm_open(h,name);}
static int pipe_config(void *h,const void *v){return nvaudio_set_pcm_info(h,v);}
static int pipe_meta(void *h,const void *s,unsigned *m){return nvaudio_create_metadata(h,s,m);}
#else
static int pipe_open(void **h,const char *name){assert(!strcmp(name,"MPL1"));*h=(void *)1;return 0;}
static int pipe_config(void *h,const void *v){const unsigned *c=v;assert(h&&c[0]==48000&&c[1]==2&&c[2]==6);return 0;}
static int pipe_meta(void *h,const void *s,unsigned *m){assert(h&&s);m[0]=1;m[1]=2;return 0;}
#endif
static int pipe_write(void *h,const void *v,unsigned bytes,const unsigned *m,unsigned timeout){
 assert(h&&v&&bytes%12==0&&timeout==20);
 const int16_t *p=v;for(unsigned i=0;i<bytes/2;i++)pipeline_energy[i%6]+=(unsigned)(p[i]<0?-p[i]:p[i]);
 ++pipeline_writes;pipeline_frames+=bytes/12;
#ifdef MHI2_PIPELINE_NATIVE
 return nvaudio_pcm_write(h,v,bytes,m,timeout);
#else
 assert(m[0]==1);return 0;
#endif
}
static int pipe_close(void *h){assert(h);
#ifdef MHI2_PIPELINE_NATIVE
 return nvaudio_pcm_close(h);
#else
 return 0;
#endif
}
static unsigned char *read_blob(const char *path,size_t *bytes){
 FILE *f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>0);rewind(f);
 unsigned char *p=malloc((size_t)n);assert(p&&fread(p,1,(size_t)n,f)==(size_t)n);fclose(f);*bytes=(size_t)n;return p;
}
static void submit(struct mhi2_nv_receiver_pump *pump,unsigned sequence,unsigned index,
                   const unsigned char *cookie,size_t cookie_size,const unsigned char *au,size_t bytes){
 unsigned char plain[MAX_FRAME],packet[MAX_FRAME];
 size_t n=build_extension(plain,(uint8_t)index,1024,index,cookie,cookie_size,au,bytes);
 n=seal_frame(packet,sequence,SPA_APAP_AAD_EXTENSION,plain,n);
 unsigned char store[MAX_FRAME];spa_apap_framer_t framer;
 spa_apap_framer_init(&framer,store,sizeof(store));size_t at=0;unsigned completed=0;
 while(at<n){
  const unsigned char *frame=0;size_t length=0,consumed=0;
  size_t take=spa_apap_framer_want(&framer,37);if(take>n-at)take=n-at;
  int status=spa_apap_framer_push(&framer,packet+at,take,&consumed,&frame,&length);
  assert(status>=0&&consumed);at+=consumed;
  if(status){assert(spa103_render_frame(frame,length,0)==SPA103_RENDER_FRAME_CONSUMED);++completed;}
 }
 assert(completed==1);
 if(spa103_render.authFailed||spa103_render.tlvFailed||spa103_render.decodeFailed)
  fprintf(stderr,"pipeline seq=%u index=%u auth=%llu tlv=%llu decode=%llu state=%d channels=%u\n",sequence,index,(unsigned long long)spa103_render.authFailed,(unsigned long long)spa103_render.tlvFailed,(unsigned long long)spa103_render.decodeFailed,spa103_render.state,spa103_render.sourceChannels);
 assert(!spa103_render.authFailed&&!spa103_render.tlvFailed&&!spa103_render.decodeFailed);
 struct mhi2_nv_result r=mhi2_nv_receiver_pump_once(pump);assert(!r.status&&!r.uncertain_frames);
}
int main(int argc,char **argv){
 assert(argc==3);assert(!setenv("AUDIO_PRIME_MS","20",1));
#ifndef MHI2_PIPELINE_NATIVE
 assert(reference_interposer_main()==0);
#else
 assert(!setenv("AUDIO_SPATIAL","1",1));assert(!setenv("AUDIO_STEREO_UPMIX","off",1));
#endif
 assert(!spa_apac_codec_get_api(1,&real_apac_api,sizeof(real_apac_api)));
 test_runtime_codec=&real_apac_api;
 g_blob=read_blob(argv[1],&g_blobBytes);assert(rd32(8)==6&&rd32(12)==6&&rd32(16)==1024);
 size_t cookie_size=rd32(20);const unsigned char *cookie=g_blob+24;
 size_t lengths=24+cookie_size,at=lengths+24;
 struct mhi2_nv_api native={pipe_open,pipe_config,pipe_meta,pipe_write,pipe_close};
 struct mhi2_nv_sink sink;struct mhi2_nv_receiver_pump pump;
 struct mhi2_buffered_source source={spa_buffered_pcm_peek,spa_buffered_pcm_commit,spa_buffered_pcm_pending};
 assert(!mhi2_nv_sink_init(&sink,&native,"MPL1",1));assert(!mhi2_nv_sink_start(&sink));
 assert(!mhi2_nv_receiver_pump_init(&pump,&sink,&source));
 memcpy(spa103_receiver.shk,kTestKey,sizeof(kTestKey));
 spa103_render_start(400,65,1,21168000,0,0,0);assert(spa103_render_set_rate(400,1));
 unsigned sequence=1;
 for(unsigned i=0;i<6;i++){unsigned n=rd32(lengths+i*4);submit(&pump,sequence++,65,cookie,cookie_size,g_blob+at,n);at+=n;}
 assert(spa103_render.sourceChannels==6&&spa103_render.formatIndex==65);
 const unsigned char aac_cookie[]={3,0x80,0x80,0x80,0x22,0,0,0,4,0x80,0x80,0x80,0x14,0x40,0x14,0,0x18,0,0,1,0xf4,0,0,1,0xf4,0,5,0x80,0x80,0x80,2,0x11,0x90,6,0x80,0x80,0x80,1,2};
 size_t aac_size;unsigned char *aac=read_blob(argv[2],&aac_size);assert(aac_size>12);
 unsigned count=(unsigned)aac[8]|((unsigned)aac[9]<<8)|((unsigned)aac[10]<<16)|((unsigned)aac[11]<<24);at=12;
 for(unsigned i=0;i<count;i++){
  assert(at+4<=aac_size);unsigned n=(unsigned)aac[at]|((unsigned)aac[at+1]<<8)|((unsigned)aac[at+2]<<16)|((unsigned)aac[at+3]<<24);at+=4;
  assert(n<=aac_size-at);submit(&pump,sequence++,23,aac_cookie,sizeof(aac_cookie),aac+at,n);at+=n;
 }
 assert(spa103_render.sourceChannels==2&&spa103_render.formatIndex==23);
 at=lengths+24;
 for(unsigned i=0;i<6;i++){unsigned n=rd32(lengths+i*4);submit(&pump,sequence++,65,cookie,cookie_size,g_blob+at,n);at+=n;}
 assert(spa103_render.sourceChannels==6&&spa103_render.formatIndex==65);
 assert(pipeline_frames==(12+count)*1024&&spa103_render.fill==0&&spa103_render.reservedReadFrames==0);
 assert(pipeline_energy[0]&&pipeline_energy[1]);
 assert(!mhi2_nv_receiver_pump_destroy(&pump));assert(!mhi2_nv_sink_destroy(&sink));spa103_render_stop();
 free(g_blob);free(aac);
 printf("NVIDIA buffered pipeline: authenticated APAP -> APAC/AAC/APAC inline switch -> renderer ring -> native API boundary PASS (%u frames, %u writes)\n",pipeline_frames,pipeline_writes);
 return 0;
}
