/* Unchanged full reference interposer suite, then its actual PCM ring coupled
 * to the NVIDIA transaction adapter. Only the native driver boundary is fake. */
#define main reference_interposer_main
#include "../reference-audio-a63d547/tests/audio_interposer_test.c"
#undef main
#include "nvidia_receiver_pump.h"
static int nv_calls,nv_fail_at;
static int nv_open(void **h,const char *name){assert(!strcmp(name,"MPL1"));*h=(void *)0x1234;return 0;}
static int nv_config(void *h,const void *p){const unsigned *c=p;assert(h&&(c[0]==48000&&c[1]==2&&c[2]==6));return 0;}
static int nv_metadata(void *h,const void *p,unsigned *m){assert(h&&p);m[0]=1;m[1]=2;return 0;}
static int nv_write(void *h,const void *p,unsigned bytes,const unsigned *m,unsigned timeout){
 assert(h&&p&&bytes==512*12&&m[0]==1&&timeout==20);
 const int16_t *samples=p;
 for(unsigned f=0;f<512;f++)for(unsigned c=0;c<6;c++)assert(samples[f*6+c]==(int)(1000*(c+1)));
 return ++nv_calls==nv_fail_at?77:0;
}
static int nv_close(void *h){assert(h);return 0;}
static void refill(void){
 spa103_render.fill=1024;spa103_render.head=0;spa103_render.primed=1;
 for(unsigned f=0;f<1024;f++)for(unsigned c=0;c<6;c++)spa103_render.ring[f*6+c]=(int16_t)(1000*(c+1));
}
int main(void){
 assert(reference_interposer_main()==0);
 struct mhi2_nv_api api={nv_open,nv_config,nv_metadata,nv_write,nv_close};
 struct mhi2_nv_sink sink;struct mhi2_nv_receiver_pump pump;
 struct mhi2_buffered_source source={spa_buffered_pcm_peek,spa_buffered_pcm_commit,spa_buffered_pcm_pending};
 assert(!mhi2_nv_sink_init(&sink,&api,"MPL1",1));assert(!mhi2_nv_sink_start(&sink));
 assert(!mhi2_nv_receiver_pump_init(&pump,&sink,&source));
 spa103_render.ring=calloc(spa103_ring_frames()*6,sizeof(int16_t));assert(spa103_render.ring);
 spa103_render.state=SPA_103_RENDER_RUNNING;spa103_render.outputChannels=6;
 spa_apap_lifecycle_init(&spa103_render.lifecycle,0);spa_apap_lifecycle_set_rate(&spa103_render.lifecycle,1);
 refill();nv_calls=0;nv_fail_at=0;
 struct mhi2_nv_result r=mhi2_nv_receiver_pump_once(&pump);
 assert(!r.status&&r.confirmed_frames==1024&&!r.uncertain_frames);
 assert(spa103_render.fill==0&&spa103_render.reservedReadFrames==0&&nv_calls==2);
 r=mhi2_nv_receiver_pump_once(&pump);assert(!r.status&&nv_calls==2);
 refill();nv_calls=0;nv_fail_at=2;
 r=mhi2_nv_receiver_pump_once(&pump);
 assert(r.status==77&&r.confirmed_frames==512&&r.uncertain_frames==512);
 assert(spa103_render.fill==512&&spa103_render.head==512&&spa103_render.reservedReadFrames==0);
 r=mhi2_nv_receiver_pump_once(&pump);assert(r.status==77&&nv_calls==2);
 assert(mhi2_nv_receiver_pump_rearm(&pump)==MHI2_NV_INVALID);
 assert(!mhi2_nv_sink_flush(&sink));
 assert(mhi2_nv_receiver_pump_rearm(&pump)==MHI2_NV_INVALID); /* queued ambiguous suffix */
 spa103_render.fill=0;spa103_render.head=0;
 assert(!mhi2_nv_receiver_pump_rearm(&pump));
 refill();nv_calls=0;nv_fail_at=0;
 assert(!mhi2_nv_receiver_pump_once(&pump).status);
 /* A native generation replaced between transactions never accepts queued PCM. */
 refill();assert(!mhi2_nv_sink_flush(&sink));nv_calls=0;
 r=mhi2_nv_receiver_pump_once(&pump);assert(r.status==MHI2_NV_STALE&&nv_calls==0);
 assert(spa103_render.fill==1024&&spa103_render.reservedReadFrames==0);
 assert(!mhi2_nv_receiver_pump_destroy(&pump));assert(!mhi2_nv_sink_destroy(&sink));
 free(spa103_render.ring);spa103_render.ring=0;spa103_render.fill=0;spa103_render.state=SPA_103_RENDER_IDLE;
 spa_apap_lifecycle_dispose(&spa103_render.lifecycle);
 puts("NVIDIA receiver: actual reference ring, known-prefix commit, ambiguous suffix fail-stop, explicit reset gate, stale generation PASS");
 return 0;
}
