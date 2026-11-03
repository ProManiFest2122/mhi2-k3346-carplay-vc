#include "nvidia_pcm_sink.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int opens,closes,writes,fail_at,config_fail,metadata_fail,close_fail;
static int handle;
static int op(void **h,const char *name){assert(!strcmp(name,"MPL1"));*h=&handle;opens++;return 0;}
static int cf(void *h,const void *p){const unsigned *v=p;assert(h==&handle&&v[0]==48000&&v[1]==2&&v[2]==6);return config_fail?8:0;}
static int md(void *h,const void *p,unsigned *m){const unsigned char *v=p;unsigned rate;memcpy(&rate,v+8,4);assert(h==&handle&&v[1]==252&&rate==48000);m[0]=7;m[1]=0x3fc00;return metadata_fail?9:0;}
static int wr(void *h,const void *p,unsigned bytes,const unsigned *m,unsigned timeout){assert(h==&handle&&p&&bytes<=512*12&&bytes%12==0&&m[1]==0x3fc00&&timeout==20);writes++;return writes==fail_at?17:0;}
static int cl(void *h){assert(h==&handle);closes++;return close_fail?19:0;}
int main(void){
 struct mhi2_nv_sink s;struct mhi2_nv_api api={op,cf,md,wr,cl};short pcm[1024*6]={0};struct mhi2_nv_result r;
 assert(mhi2_nv_sink_init(&s,&api,"MPL1",0)==MHI2_NV_UNQUALIFIED&&opens==0);
 assert(!mhi2_nv_sink_init(&s,&api,"MPL1",1));assert(!mhi2_nv_sink_start(&s));unsigned generation=mhi2_nv_sink_generation(&s);
 r=mhi2_nv_sink_write(&s,generation,pcm,1024);assert(r.status==0&&r.confirmed_frames==1024&&r.uncertain_frames==0&&writes==2);
 fail_at=4;r=mhi2_nv_sink_write(&s,generation,pcm,1024);assert(r.status==17&&r.confirmed_frames==512&&r.uncertain_frames==512);
 r=mhi2_nv_sink_write(&s,generation,pcm,1024);assert(r.status==17&&r.confirmed_frames==0&&r.uncertain_frames==0&&writes==4);
 assert(!mhi2_nv_sink_flush(&s));assert(mhi2_nv_sink_generation(&s)!=generation);
 r=mhi2_nv_sink_write(&s,generation,pcm,512);assert(r.status==MHI2_NV_STALE&&writes==4);
 generation=mhi2_nv_sink_generation(&s);r=mhi2_nv_sink_write(&s,generation,0,1);assert(r.status==MHI2_NV_INVALID&&writes==4);
 r=mhi2_nv_sink_write(&s,generation,pcm,1025);assert(r.status==MHI2_NV_INVALID&&writes==4);
 r=mhi2_nv_sink_write(&s,generation,pcm,512);assert(!r.status&&r.confirmed_frames==512);
 close_fail=1;assert(mhi2_nv_sink_flush(&s)==19);int old_opens=opens;r=mhi2_nv_sink_write(&s,mhi2_nv_sink_generation(&s),pcm,512);assert(r.status==19&&opens==old_opens);
 close_fail=0;assert(!mhi2_nv_sink_flush(&s));assert(!mhi2_nv_sink_destroy(&s));assert(opens==closes-1); /* failed close counted, no ownership loss */
 config_fail=1;assert(!mhi2_nv_sink_init(&s,&api,"MPL1",1));assert(mhi2_nv_sink_start(&s)==8&&!s.handle);assert(!mhi2_nv_sink_destroy(&s));config_fail=0;
 metadata_fail=1;assert(!mhi2_nv_sink_init(&s,&api,"MPL1",1));assert(mhi2_nv_sink_start(&s)==9&&!s.handle);assert(!mhi2_nv_sink_destroy(&s));
 puts("NVIDIA sink: route gate, known prefix/uncertain range, fail latch, generation reset, bounds and cleanup PASS");
}
