#include "nvidia_pcm_sink.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond=PTHREAD_COND_INITIALIZER;
static int paused,released,calls,closes,handle;
static struct mhi2_nv_sink sink;
static struct mhi2_nv_result outcome;
static short pcm[1024*6];
static unsigned generation;
static int open_device(void **h,const char *name){(void)name;*h=&handle;return 0;}
static int configure(void *h,const void *p){(void)h;(void)p;return 0;}
static int metadata(void *h,const void *p,unsigned *m){(void)h;(void)p;m[0]=m[1]=0;return 0;}
static int write_device(void *h,const void *p,unsigned n,const unsigned *m,unsigned t){
 (void)h;(void)p;(void)n;(void)m;(void)t;
 pthread_mutex_lock(&lock);assert(closes==0);calls++;
 if(calls==1){paused=1;pthread_cond_broadcast(&cond);while(!released)pthread_cond_wait(&cond,&lock);}
 pthread_mutex_unlock(&lock);return 0;
}
static int close_device(void *h){(void)h;pthread_mutex_lock(&lock);assert(calls==2);closes++;pthread_mutex_unlock(&lock);return 0;}
static void *writer(void *p){(void)p;outcome=mhi2_nv_sink_write(&sink,generation,pcm,1024);return 0;}
static void *flusher(void *p){(void)p;assert(!mhi2_nv_sink_flush(&sink));return 0;}
int main(void){
 struct mhi2_nv_api api={open_device,configure,metadata,write_device,close_device};pthread_t w,f;
 assert(!mhi2_nv_sink_init(&sink,&api,"MPL1",1));assert(!mhi2_nv_sink_start(&sink));generation=mhi2_nv_sink_generation(&sink);
 assert(!pthread_create(&w,0,writer,0));pthread_mutex_lock(&lock);while(!paused)pthread_cond_wait(&cond,&lock);pthread_mutex_unlock(&lock);
 assert(!pthread_create(&f,0,flusher,0));pthread_mutex_lock(&lock);released=1;pthread_cond_broadcast(&cond);pthread_mutex_unlock(&lock);
 pthread_join(w,0);pthread_join(f,0);assert(!outcome.status&&outcome.confirmed_frames==1024&&closes==1);
 assert(mhi2_nv_sink_write(&sink,generation,pcm,512).status==MHI2_NV_STALE);assert(!mhi2_nv_sink_destroy(&sink));
 puts("NVIDIA sink: concurrent write/flush serialized; stale generation never reaches driver PASS");
}
