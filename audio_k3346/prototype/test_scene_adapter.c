#include "mhi2_scene_adapter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define FRAMES 49152
static struct mhi2_scene_adapter adapter;
static short source[FRAMES*2], reference[FRAMES*6], result[FRAMES*6];
static const unsigned sizes[]={1,100,257,512,768,1024};
int main(void) {
 for(unsigned i=0;i<FRAMES;i++){source[i*2]=(short)(((int)(i%97)-48)*61);source[i*2+1]=(short)(((int)(i%137)-68)*47);}
 assert(!mhi2_scene_adapter_init(&adapter));
 assert(!mhi2_scene_adapter_mode(&adapter,1,0));
 for(unsigned i=0;i<FRAMES;i+=512)assert(!mhi2_scene_adapter_process(&adapter,source+i*2,512,reference+i*6));
 for(unsigned j=0;j<sizeof(sizes)/sizeof(*sizes);j++){
  mhi2_scene_adapter_reset(&adapter);
  for(unsigned i=0;i<FRAMES;){unsigned n=sizes[j];if(n>FRAMES-i)n=FRAMES-i;assert(!mhi2_scene_adapter_process(&adapter,source+i*2,n,result+i*6));i+=n;}
  assert(!memcmp(reference,result,sizeof(reference)));
 }
 for(unsigned i=0;i<MHI2_AUDIO_LATENCY_FRAMES*6;i++)assert(reference[i]==0);
 assert(!adapter.invalid&&!adapter.clipped);
 assert(!mhi2_scene_adapter_mode(&adapter,0,0));
 assert(!mhi2_scene_adapter_process(&adapter,source,1024,result));
 for(unsigned i=0;i<1024;i++){assert(result[i*6]==source[i*2]&&result[i*6+1]==source[i*2+1]);for(unsigned c=2;c<6;c++)assert(!result[i*6+c]);}
 assert(!mhi2_scene_adapter_mode(&adapter,1,1));
 memset(result,1,sizeof(result));assert(!mhi2_scene_adapter_process(&adapter,source,1024,result));
 for(unsigned i=0;i<1024*6;i++)assert(!result[i]);
 assert(!mhi2_scene_adapter_mode(&adapter,1,0));
 assert(!mhi2_scene_adapter_process(&adapter,source,1024,result));
 for(unsigned i=0;i<1024*6;i++)assert(!result[i]);
 unsigned before=adapter.count;
 assert(mhi2_scene_adapter_process(&adapter,source,1025,result)==-1);
 assert(adapter.count==before);
 assert(mhi2_scene_adapter_mode(&adapter,2,0)==-1);
 puts("PASS: six packet sizes bit-identical; OFF exact; 1024-frame padding; mute/reset; invalid size rejected");
}
