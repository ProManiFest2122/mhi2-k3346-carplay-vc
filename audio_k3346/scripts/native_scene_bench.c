/* SCENE CPU benchmark (isolated, no audio device): per-period processing time of the reference stereo upmixer at the
 * realtime period size (768 frames = 16 ms at 48 kHz), levels 2 and 4, with and without the mask net, synchronous.
 * Prints one line per configuration: mean / p50 / p95 / p99 / max in microseconds and the share of the 16 ms period.
 * Host build: -DMHI2_HOST.  QNX build: runs on the unit from /tmp (RAM) without installing anything. */
#include "stereo_upmix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
extern const unsigned char full_scene_mask[],full_scene_mask_end[];
#define PERIOD 768u
#define PERIODS 400u
static struct spa_stereo_upmix state;
static short input[PERIOD*2],output[PERIOD*6];
static unsigned long long us[PERIODS];
static void say(const char *fmt,...) __attribute__((format(printf,1,2)));
static void say(const char *fmt,...){char b[320];va_list ap;va_start(ap,fmt);int n=vsnprintf(b,sizeof(b),fmt,ap);va_end(ap);if(n>0)write(1,b,(size_t)n);} /* write(), not stdio: the QNX build has no libc init */
static unsigned long long now_us(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (unsigned long long)t.tv_sec*1000000ull+(unsigned long long)t.tv_nsec/1000ull;}
static int cmp(const void *a,const void *b){unsigned long long x=*(const unsigned long long *)a,y=*(const unsigned long long *)b;return x<y?-1:x>y;}
static void bench(const char *name,unsigned level,int mask){
 struct spa_stereo_upmix_config cfg;spa_stereo_upmix_default_config(&cfg);
 cfg.backend=SPA_STEREO_UPMIX_SCENE;cfg.level=level;
 if(mask)spa_stereo_upmix_config_set_scene_mask_blob(&cfg,full_scene_mask,(size_t)(full_scene_mask_end-full_scene_mask));
 memset(&state,0,sizeof(state));
 if(spa_stereo_upmix_init(&state,&cfg)){say("SCENE_BENCH %s init_failed\n",name);return;}
 unsigned long long sum=0;
 for(unsigned p=0;p<PERIODS+20;p++){
  for(unsigned i=0;i<PERIOD;i++){unsigned n=p*PERIOD+i;input[2*i]=(short)(((int)(n%197)-98)*97+((int)(n%53)-26)*40);input[2*i+1]=(short)(((int)(n%337)-168)*53+((int)(n%41)-20)*40);}
  unsigned long long t0=now_us();
  if(spa_stereo_upmix_process_s16(&state,input,PERIOD,output,PERIOD)){say("SCENE_BENCH %s process_failed\n",name);return;}
  unsigned long long dt=now_us()-t0;
  if(p>=20){us[p-20]=dt;sum+=dt;} /* first 20 periods warm up (zero-latency fill, FFT tables) */
 }
 qsort(us,PERIODS,sizeof(us[0]),cmp);
 say("SCENE_BENCH %s mean_us=%llu p50=%llu p95=%llu p99=%llu max=%llu period_us=16000 mean_share_pct=%llu\n",name,sum/PERIODS,us[PERIODS/2],us[PERIODS*95/100],us[PERIODS*99/100],us[PERIODS-1],sum/PERIODS*100/16000);

 spa_stereo_upmix_reset(&state);
}
#ifdef MHI2_HOST
int main(void){
#else
void _start(void){
#endif
 bench("level2_nomask",2,0);bench("level2_mask",2,1);bench("level4_nomask",4,0);bench("level4_mask",4,1);
 say("SCENE_BENCH done\n");
#ifdef MHI2_HOST
 return 0;
#else
 exit(0);
#endif
}
