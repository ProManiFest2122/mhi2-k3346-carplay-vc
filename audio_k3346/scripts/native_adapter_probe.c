/* Isolated QNX/ARM renderer and native sink probe. Not an AirPlay interposer. */
#include "mhi2_scene_adapter.h"
extern int nvaudio_pcm_open(void **, const char *), nvaudio_set_pcm_info(void *, const void *);
extern int nvaudio_create_metadata(void *, const void *, unsigned *);
extern int nvaudio_pcm_write(void *, const void *, unsigned, const unsigned *, unsigned);
extern int nvaudio_pcm_close(void *), usleep(unsigned), write(int,const void *,size_t);
extern int snprintf(char *,size_t,const char *,...);
extern void exit(int);
static struct mhi2_scene_adapter state;
static short input[512*2];
static short pcm[1024*6];
static unsigned energy[6], clipped, invalid;
void _start(void) {
 int status=mhi2_scene_adapter_init(&state);
 mhi2_scene_adapter_mode(&state,1,0);
 if(status) exit(1);
 void *sink=0; unsigned config[3]={48000,2,6}, meta[2]={0};
 unsigned char sad[20]={0,252,0,1,16,16}; *(unsigned *)(sad+8)=48000;
 status=nvaudio_pcm_open(&sink,"MPL1"); if(status) exit(2);
 if(nvaudio_set_pcm_info(sink,config)||nvaudio_create_metadata(sink,sad,meta)) exit(3);
 unsigned writes=0, errors=0, frames=0;
 for(unsigned b=0;b<192;b++) {
  for(unsigned i=0;i<512;i++) {
   unsigned t=b*512+i;
   input[i*2]=(short)(((int)(t%97)-48)*61);
   input[i*2+1]=(short)(((int)(t%137)-68)*47);
  }
  unsigned n=512;
  if(mhi2_scene_adapter_process(&state,input,n,pcm)) exit(5);
  for(unsigned i=0;i<n*6;i++) energy[i%6]+=(unsigned)(pcm[i]<0?-pcm[i]:pcm[i]);
  if(n) {status=nvaudio_pcm_write(sink,pcm,n*12,meta,30); writes+=!status;errors+=!!status;frames+=n;}
  usleep(12000);
 }
 nvaudio_pcm_close(sink);
 clipped=state.clipped;invalid=state.invalid;
 char text[384]; int n=snprintf(text,sizeof(text),"ADAPTER_NATIVE frames=%u writes=%u errors=%u invalid=%u clipped=%u energy=%u,%u,%u,%u,%u,%u\n",frames,writes,errors,invalid,clipped,energy[0],energy[1],energy[2],energy[3],energy[4],energy[5]);write(1,text,n);
 clipped=state.clipped; invalid=state.invalid;
 mhi2_scene_adapter_reset(&state);
 for(unsigned i=0;i<1024;i++) input[i]=0;
 unsigned silence=0;
 for(unsigned b=0;b<8;b++) {unsigned n=512;if(mhi2_scene_adapter_process(&state,input,n,pcm))exit(5);for(unsigned i=0;i<n*6;i++) silence+=pcm[i]!=0;}
 n=snprintf(text,sizeof(text),"ADAPTER_NATIVE_RESET nonzero=%u\n",silence);write(1,text,n);
 exit(errors||invalid||clipped||silence?4:0);
}
