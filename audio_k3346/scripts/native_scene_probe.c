/* Isolated QNX/ARM renderer and native sink probe. Not an AirPlay interposer. */
#include "scene_engine.h"
extern int nvaudio_pcm_open(void **, const char *), nvaudio_set_pcm_info(void *, const void *);
extern int nvaudio_create_metadata(void *, const void *, unsigned *);
extern int nvaudio_pcm_write(void *, const void *, unsigned, const unsigned *, unsigned);
extern int nvaudio_pcm_close(void *), usleep(unsigned), write(int,const void *,size_t);
extern int snprintf(char *,size_t,const char *,...);
extern void exit(int);
static struct scene_state state;
static float input[512*2], output[1024*6];
static short pcm[1024*6];
static unsigned energy[6], clipped, invalid;
void _start(void) {
 struct scene_config cfg; scene_config_defaults(&cfg);
 int status=scene_engine_init(&state,&cfg);
 if(status) exit(1);
 void *sink=0; unsigned config[3]={48000,2,6}, meta[2]={0};
 unsigned char sad[20]={0,252,0,1,16,16}; *(unsigned *)(sad+8)=48000;
 status=nvaudio_pcm_open(&sink,"MPL1"); if(status) exit(2);
 if(nvaudio_set_pcm_info(sink,config)||nvaudio_create_metadata(sink,sad,meta)) exit(3);
 unsigned writes=0, errors=0, frames=0;
 for(unsigned b=0;b<192;b++) {
  for(unsigned i=0;i<512;i++) {
   unsigned t=b*512+i;
   input[i*2]=((int)(t%97)-48)/384.0f;
   input[i*2+1]=((int)(t%137)-68)/544.0f;
  }
  unsigned n=scene_engine_push(&state,input,512,output,1024);
  for(unsigned i=0;i<n*6;i++) {
   float v=output[i]*32768.0f;
   if(v!=v) {invalid++;v=0;}
   if(v>32767) {clipped++;v=32767;} if(v< -32768) {clipped++;v=-32768;}
   pcm[i]=(short)v; energy[i%6]+=(unsigned)(pcm[i]<0?-pcm[i]:pcm[i]);
  }
  if(n) {status=nvaudio_pcm_write(sink,pcm,n*12,meta,30); writes+=!status;errors+=!!status;frames+=n;}
  usleep(12000);
 }
 nvaudio_pcm_close(sink);
 char text[384]; int n=snprintf(text,sizeof(text),"SCENE_NATIVE frames=%u writes=%u errors=%u invalid=%u clipped=%u energy=%u,%u,%u,%u,%u,%u\n",frames,writes,errors,invalid,clipped,energy[0],energy[1],energy[2],energy[3],energy[4],energy[5]);write(1,text,n);
 scene_engine_reset_state(&state);
 for(unsigned i=0;i<1024;i++) input[i]=0;
 unsigned silence=0;
 for(unsigned b=0;b<8;b++) {unsigned n=scene_engine_push(&state,input,512,output,1024);for(unsigned i=0;i<n*6;i++) silence+=output[i]!=0;}
 n=snprintf(text,sizeof(text),"SCENE_NATIVE_RESET nonzero=%u\n",silence);write(1,text,n);
 exit(errors||invalid||clipped||silence?4:0);
}
