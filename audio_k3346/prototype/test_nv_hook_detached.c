/* K3346 NVIDIA hook variant: the reference detached/cold buffered logic drives the
 * NVIDIA shim.  Native API is mocked (no QNX).  Checks at-most-once recovery. */
#define ENABLE_LOGGING 1
#define ENABLE_STATE_TRACE 0
#include <assert.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
static void *test_dlsym(void *, const char *);
#define dlsym test_dlsym
#include "../artifacts/full-audio/port/audio_hook_k3346_nv.c"
#undef dlsym
#include <stdarg.h>
void log_write(log_level_t level,const char *module,const char *fmt,...){(void)level;fprintf(stderr,"[%s] ",module);va_list a;va_start(a,fmt);vfprintf(stderr,fmt,a);va_end(a);fputc(10,stderr);}
bool log_verbose(void){return false;}
static const spa_apac_codec_api_t *test_runtime_codec;
const spa_apac_codec_api_t *spa_apac_runtime_load(const char **path){(void)path;return test_runtime_codec;}
#define main reference_type103_main
#include "../reference-audio-a63d547/tests/apac_type103_test.c"
#undef main
static spa_apac_codec_api_t real_apac_api;
enum {MAXW=256};
static unsigned nv_opens,nv_closes,nv_cfg,nv_writes,nv_fail_at,nv_fail_count,nv_writes_total,nv_frames,silent_writes,route_requests;
static unsigned long long sums[MAXW];static unsigned nsums;
#define MAXWR 4096
static unsigned long long wr_front[MAXWR],wr_surr[MAXWR];static unsigned nwr;
#ifdef MHI2_PIPELINE_NATIVE
static volatile unsigned *tdm_marker;
static void tdm_tag(unsigned v){if(!tdm_marker){int fd=open("/dev/mem",2);void *m=fd>=0?mmap64(0,4096,0x700,1,fd,0x70080000ull):(void *)-1;if(fd>=0)close(fd);if(m==(void *)-1){printf("tdm marker map failed fd=%d\n",fd);return;}tdm_marker=(volatile unsigned *)((char *)m+0xe20);}*tdm_marker=v;printf("tdm marker=%u\n",v);}
#else
static void tdm_tag(unsigned v){(void)v;}
#endif
static FILE *nv_dump;
static void dump_rec(unsigned type,unsigned frames,const void *pcm){if(!nv_dump)return;fwrite(&type,4,1,nv_dump);fwrite(&frames,4,1,nv_dump);if(pcm)fwrite(pcm,12,frames,nv_dump);}
static int16_t cap_l[90000],cap_r[90000];static unsigned cap_n;static int cap_on;
static char nv_token;static char nv_device[16];
#ifdef MHI2_PIPELINE_NATIVE
extern int nvaudio_pcm_open(void **,const char *),nvaudio_set_pcm_info(void *,const void *),nvaudio_create_metadata(void *,const void *,unsigned *),nvaudio_pcm_write(void *,const void *,unsigned,const unsigned *,unsigned),nvaudio_pcm_close(void *);
static int nv_open(void **h,const char *n){strncpy(nv_device,n,15);++nv_opens;return nvaudio_pcm_open(h,n);}
static int nv_config(void *h,const void *v){++nv_cfg;return nvaudio_set_pcm_info(h,v);}
static int nv_meta(void *h,const void *s,unsigned *m){return nvaudio_create_metadata(h,s,m);}
#else
static int nv_open(void **h,const char *n){assert(!*h||1);*h=&nv_token;strncpy(nv_device,n,15);++nv_opens;return 0;}
static int nv_config(void *h,const void *v){assert(h);(void)v;++nv_cfg;return 0;}
static int nv_meta(void *h,const void *s,unsigned *m){assert(h&&s);m[0]=1;m[1]=2;return 0;}
#endif
static int nv_passthrough; /* set by the stress test while the bridge forwards a non-claimed format (16k mono) to the real device */
static int nv_write(void *h,const void *v,unsigned bytes,const unsigned *m,unsigned t){
 if(nv_passthrough){(void)m;(void)t;assert(h&&v);return 0;}
 if(getenv("NV_DEBUGW")&&!(h&&v&&bytes%12==0&&t==20))fprintf(stderr,"nv_write odd: bytes=%u t=%u\n",bytes,t);
 assert(h&&v&&bytes%12==0&&t==20);++nv_writes_total;
 if(nv_fail_at&&nv_writes_total>=nv_fail_at&&nv_writes_total<nv_fail_at+(nv_fail_count?nv_fail_count:1)){dump_rec(2,bytes/12,0);return -5;}
 const int16_t *p=v;unsigned long long sum=0;int zero=1;
 for(unsigned i=0;i<bytes/2;i++){if(p[i])zero=0;sum=sum*1315423911ull+(unsigned)(p[i]+40000);}
 if(zero)++silent_writes;else if(!getenv("MHI2_NV_SCENARIO")){assert(nsums<MAXW);for(unsigned k=0;k<nsums;k++)assert(sums[k]!=sum);/* no replay */sums[nsums++]=sum;}
 if(nwr<MAXWR){unsigned long long f=0,r=0;for(unsigned i=0;i<bytes/2;i++){unsigned a=(unsigned)(p[i]<0?-p[i]:p[i]);if(i%6<2)f+=a;else r+=a;}wr_front[nwr]=f;wr_surr[nwr]=r;++nwr;}
 const void *out=v;static int16_t wmbuf[1024*6];
 /* research watermark for capture alignment: C = write sequence, LFE = frame index+1 (APAC fixture leaves both silent) */
 if(getenv("NV_WATERMARK")){assert(bytes<=sizeof(wmbuf));memcpy(wmbuf,v,bytes);for(unsigned f=0;f<bytes/12;f++){wmbuf[f*6+2]=(int16_t)nv_writes_total;wmbuf[f*6+3]=(int16_t)(f+1);}out=wmbuf;}
 {int z=1;for(unsigned i=0;i<bytes/2;i++)if(p[i]){z=0;break;}dump_rec(z?1:0,bytes/12,out);}
 if(cap_on)for(unsigned f=0;f<bytes/12&&cap_n<90000;f++){cap_l[cap_n]=p[f*6];cap_r[cap_n]=p[f*6+1];++cap_n;}
 ++nv_writes;nv_frames+=bytes/12;
#ifdef MHI2_PIPELINE_NATIVE
 {int wr=nvaudio_pcm_write(h,out,bytes,m,t);
  if(getenv("NV_PACE"))usleep((unsigned)((unsigned long long)(bytes/12)*1000000ull/48000ull));/* emulate device-rate pacing */
  return wr;}
#else
 return 0;
#endif
}
static int nv_close(void *h){assert(h);++nv_closes;
#ifdef MHI2_PIPELINE_NATIVE
 return nvaudio_pcm_close(h);
#else
 return 0;
#endif
}
static void cb_type(int a,void *b){(void)a;(void)b;++route_requests;}
static void cb_down(void *b){(void)b;}
static void *test_dlsym(void *scope,const char *name){(void)scope;
 if(!strncmp(name,"nvaudio_",8))return dlsym(scope,name); /* real libnvaudio (macro is undefined here) */
 if(!strcmp(name,SPA_DIO_AUDIO_TYPE_CALLBACK_SYM))return (void *)(uintptr_t)cb_type;
 if(!strcmp(name,SPA_DIO_AUDIO_TORN_DOWN_CALLBACK_SYM))return (void *)(uintptr_t)cb_down;
 return 0;}
static unsigned char *read_blob(const char *path,size_t *bytes){
 FILE *f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>0);rewind(f);
 unsigned char *p=malloc((size_t)n);assert(p&&fread(p,1,(size_t)n,f)==(size_t)n);fclose(f);*bytes=(size_t)n;return p;
}

static float g_loud[3]={-6.5f,-1.0f,0.96875f};
static size_t build_ext2(unsigned char *out,uint8_t idx,uint64_t cookie_id,const unsigned char *cookie,size_t cl,const unsigned char *au,size_t al){
 size_t n=build_extension(out,idx,1024,cookie_id,cookie,cl,au,al);
 /* patch the 12 loudness bytes written by the reference builder (after the cookie TLV) */
 size_t at=0;while(out[at]!=SPA_APAP_TLV_LOUDNESS_PARAMETERS){unsigned char t=out[at];(void)t;at+=2+out[at+1];}
 assert(out[at+1]==12);for(int i=0;i<3;i++){uint32_t bits;memcpy(&bits,&g_loud[i],4);put_be32(out+at+2+4*i,bits);}
 return n;}
static void feed_idx(unsigned idx,unsigned long long cookie_id,unsigned seq,const unsigned char *cookie,size_t cs,const unsigned char *au,size_t n){
 unsigned char plain[MAX_FRAME],packet[MAX_FRAME];
 size_t len=build_ext2(plain,(uint8_t)idx,cookie_id,cookie,cs,au,n);
 len=seal_frame(packet,seq,SPA_APAP_AAD_EXTENSION,plain,len);
 unsigned char store[MAX_FRAME];spa_apap_framer_t framer;spa_apap_framer_init(&framer,store,sizeof(store));
 size_t at=0;unsigned done=0;
 while(at<len){const unsigned char *frame=0;size_t fl=0,used=0;size_t take=spa_apap_framer_want(&framer,37);if(take>len-at)take=len-at;
  int st=spa_apap_framer_push(&framer,packet+at,take,&used,&frame,&fl);assert(st>=0&&used);at+=used;
  if(st){assert(spa103_render_frame(frame,fl,0)==SPA103_RENDER_FRAME_CONSUMED);++done;}}
 assert(done==1);
}
static void feed(unsigned seq,const unsigned char *cookie,size_t cs,const unsigned char *au,size_t n){feed_idx(65,seq-1,seq,cookie,cs,au,n);}
static void drain(unsigned max){for(unsigned i=0;i<max;i++)spa_pcm_sink_write_one_detached_period();}

#include <unistd.h>
static void set_level_file(const char *text){FILE *f=fopen("/tmp/mhi2_audio_level","w");assert(f);fputs(text,f);fclose(f);}
static unsigned char *aac_blob;static size_t aac_bytes,aac_at,aac_count,aac_left;static unsigned aac_seq;
static const unsigned char aac_cookie[]={3,0x80,0x80,0x80,0x22,0,0,0,4,0x80,0x80,0x80,0x14,0x40,0x14,0,0x18,0,0,1,0xf4,0,0,1,0xf4,0,5,0x80,0x80,0x80,2,0x11,0x90,6,0x80,0x80,0x80,1,2};
static void feed_aac(unsigned count){
 for(unsigned i=0;i<count;i++){
  if(!aac_left){aac_at=12;aac_left=aac_count;}
  unsigned n=(unsigned)aac_blob[aac_at]|((unsigned)aac_blob[aac_at+1]<<8)|((unsigned)aac_blob[aac_at+2]<<16)|((unsigned)aac_blob[aac_at+3]<<24);aac_at+=4;
  feed_idx(23,23,aac_seq++,aac_cookie,sizeof(aac_cookie),aac_blob+aac_at,n);
  if(getenv("NV_DEBUG")){int16_t tmp[768*6];size_t got=spa_buffered_pcm_peek(tmp,768,6);int m=0;for(size_t k=0;k<got*6;k++){int a=tmp[k]<0?-tmp[k]:tmp[k];if(a>m)m=a;}printf("au n=%u got=%u peakout=%d pending=%u\n",n,(unsigned)got,m,(unsigned)spa_buffered_pcm_pending(6));spa_buffered_pcm_commit(0,6);}
  aac_at+=n;--aac_left;
  for(unsigned k=0;k<4&&spa_buffered_pcm_pending(6)>=768;k++)spa_pcm_sink_write_one_detached_period();
 }
 drain(8);
}
static unsigned long long window_sum(unsigned long long *a,unsigned from,unsigned to){unsigned long long t=0;for(unsigned i=from;i<to&&i<nwr;i++)t+=a[i];return t;}
/* Live MMI level through the polled file: OFF -> AGGRESSIVE -> OFF on a running stereo stream. */
static int run_level_scenario(void){
 assert(spa_upmix_level==SPA_UPMIX_LEVEL_OFF||1);
 assert(spa_pcm_sink_bootstrap_buffered()==1);
 feed_aac(10);
 if(getenv("NV_DEBUG"))printf("render: framesIn=%llu decodeFailed=%llu upmixFailed=%llu state=%d ch=%u idx=%d\n",(unsigned long long)spa103_render.framesIn,(unsigned long long)spa103_render.decodeFailed,(unsigned long long)spa103_render.upmixFailed,(int)spa103_render.state,spa103_render.sourceChannels,(int)spa103_render.formatIndex);
 unsigned a_end=nwr;assert(a_end>8);if(getenv("NV_DEBUG"))for(unsigned i=0;i<a_end;i++)printf("a%u front=%llu surr=%llu\n",i,wr_front[i],wr_surr[i]);
 assert(window_sum(wr_front,0,a_end)>0);
 /* level OFF: bypass, every non-front channel digital zero (after the 2 priming APAC AUs) */
 printf("A(off): writes=%u front=%llu surround=%llu level=%d\n",a_end,window_sum(wr_front,0,a_end),window_sum(wr_surr,a_end/2,a_end),spa_upmix_level);
 assert(spa_upmix_level==0&&window_sum(wr_surr,a_end/2,a_end)==0);
 /* rate limit: a change inside the poll window is not seen yet */
 set_level_file("4");(void)spa_upmix_level_poll();assert(spa_upmix_level==0);
 usleep(250000);(void)spa_upmix_level_poll();assert(spa_upmix_level==SPA_UPMIX_LEVEL_AGGRESSIVE);
 unsigned b0=nwr;feed_aac(40);unsigned b_end=nwr;
 unsigned long long peak=0;for(unsigned i=b0;i<b_end;i++)if(wr_surr[i]>peak)peak=wr_surr[i];if(getenv("NV_DEBUG"))for(unsigned i=b0;i<b_end;i++)printf("w%u front=%llu surr=%llu\n",i-b0,wr_front[i],wr_surr[i]);
 unsigned first=0,reach=0;for(unsigned i=b0;i<b_end;i++){if(!first&&wr_surr[i]>peak/50)first=i;if(!reach&&wr_surr[i]>=peak*9/10){reach=i;break;}}
 printf("B(aggressive): writes=%u peak_surround=%llu first=%u reach90=%u level=%d\n",b_end-b0,peak,first-b0,reach-b0,spa_upmix_level);
 assert(peak>0&&first&&reach>first&&reach-first>=3&&reach-first<=40); /* ~300 ms glide, not a jump */
 /* invalid content is ignored, level stays */
 usleep(250000);set_level_file("9");(void)spa_upmix_level_poll();assert(spa_upmix_level==SPA_UPMIX_LEVEL_AGGRESSIVE);
 usleep(250000);set_level_file("0");(void)spa_upmix_level_poll();assert(spa_upmix_level==0);
 unsigned c0=nwr;feed_aac(40);unsigned c_end=nwr;
 printf("C(off again): writes=%u tail_surround=%llu\n",c_end-c0,window_sum(wr_surr,c_end-4,c_end));
 assert(window_sum(wr_surr,c_end-4,c_end)==0);
 /* the same live source feeds a clean shutdown */
 spa_pcm_sink_close_detached("level-test");spa103_render_stop();
 printf("NVIDIA hook live level: file poll + 300 ms glide PASS (%u native writes)\n",nwr);
 return 0;
}

/* SoundCheck: setup target + stream loudness metadata (Pioneer tuple in every packet) ramp the gain;
 * run twice (target absent vs present) on the same input and compare per-write energy. */
static unsigned long long total_e[2][MAXWR];static unsigned total_n[2];
static int run_soundcheck_scenario(const unsigned char *cookie,size_t cookie_size,size_t lengths,unsigned *seqp){
 const float params[3]={-6.5f,-1.0f,0.96875f};int rounded=0;float gain=1.0f;
 assert(spa_soundcheck_main_gain(-16,params,&rounded,&gain));
 printf("soundcheck expected: target=-16dB rounded=%d linear=%.4f\n",rounded,gain);
 for(unsigned run=0;run<2;run++){
  unsigned base=nwr;
  spa103_render_start(400+run,65,1,21168000,run?1:0,1,-16);assert(spa103_render_set_rate(400+run,1));
  size_t at=lengths+24;for(unsigned i=0;i<6;i++){unsigned n=rd32(lengths+i*4);feed(*seqp,cookie,cookie_size,g_blob+at,n);++*seqp;at+=n;}
  aac_seq=*seqp;aac_left=0;
  assert(spa_pcm_sink_bootstrap_buffered()==1);
  if(run){feed_aac(8);g_loud[0]=-2.0f;g_loud[2]=0.90f;feed_aac(10);g_loud[0]=-6.5f;g_loud[2]=0.96875f;}else feed_aac(18);
  *seqp=aac_seq;
  total_n[run]=nwr-base;assert(total_n[run]>20&&total_n[run]<MAXWR);
  for(unsigned i=0;i<total_n[run];i++)total_e[run][i]=wr_front[base+i]+wr_surr[base+i];
  spa_pcm_sink_close_detached("soundcheck-run");spa103_render_stop();
 }
 unsigned n=total_n[0]<total_n[1]?total_n[0]:total_n[1];
 double first=0,last=0;unsigned counted=0;double prev=1.0;double maxstep=0;
 for(unsigned i=0;i<n;i++){if(total_e[0][i]<2000)continue;double r=(double)total_e[1][i]/(double)total_e[0][i];
  if(!counted)first=r;last=r;++counted;if(counted>1){double st=r>prev?r-prev:prev-r;if(st>maxstep)maxstep=st;}prev=r;}
 printf("soundcheck: writes=%u counted=%u ratio first=%.3f last=%.3f maxstep=%.3f\n",n,counted,first,last,maxstep);
 const float p2[3]={-2.0f,-1.0f,0.90f};int r2=0;float gain2=1.0f;assert(spa_soundcheck_main_gain(-16,p2,&r2,&gain2));
 printf("soundcheck second tuple: rounded=%d linear=%.4f\n",r2,gain2);
 assert(counted>10&&gain<0.99f&&gain2<0.99f&&(gain2>gain*1.05||gain2<gain*0.95));
 assert(first>gain*0.97&&first<gain*1.03); /* the setup target applies from the first period */
 assert(last>gain2*0.97&&last<gain2*1.03); /* settled at the changed metadata gain */
 assert(maxstep<0.5*(gain2>gain?gain2-gain:gain-gain2)); /* metadata change ramps, no single jump */
 printf("NVIDIA hook SoundCheck: setup target + loudness ramp PASS\n");
 return 0;
}

/* Live APAC <-> AAC switching through the NVIDIA hook path: one sink connection, constant 6ch output. */
static unsigned apac_idx;static size_t apac_lengths,apac_cookie_size;static const unsigned char *apac_cookie;
static void feed_apac(unsigned count,unsigned *seqp){
 for(unsigned i=0;i<count;i++){
  size_t at=apac_lengths+24;unsigned k=apac_idx++%6;for(unsigned j=0;j<k;j++)at+=rd32(apac_lengths+j*4);
  unsigned n=rd32(apac_lengths+k*4);feed(*seqp,apac_cookie,apac_cookie_size,g_blob+at,n);++*seqp;
  for(unsigned q=0;q<4&&spa_buffered_pcm_pending(6)>=768;q++)spa_pcm_sink_write_one_detached_period();
 }
 drain(8);
}
static int run_switch_scenario(unsigned *seqp){
 struct {const char *name;int aac;unsigned n;unsigned w0,w1;unsigned long long front,surr;} seg[6];
 memset(seg,0,sizeof(seg));
 for(unsigned i=0;i<6;i++){seg[i].aac=i&1;seg[i].name=i&1?"aac":"apac";seg[i].n=i&1?12:6;}
 assert(spa_pcm_sink_bootstrap_buffered()==1);
 aac_seq=*seqp;
 for(unsigned i=0;i<6;i++){
  seg[i].w0=nwr;
  if(seg[i].aac){feed_aac(seg[i].n);*seqp=aac_seq;}else{feed_apac(seg[i].n,seqp);aac_seq=*seqp;}
  seg[i].w1=nwr;seg[i].front=window_sum(wr_front,seg[i].w0,seg[i].w1);seg[i].surr=window_sum(wr_surr,seg[i].w0+(seg[i].aac?6:0),seg[i].w1);
  printf("segment %u %s: writes=%u front=%llu surround=%llu format=%d channels=%u\n",i,seg[i].name,seg[i].w1-seg[i].w0,seg[i].front,seg[i].surr,(int)spa103_render.formatIndex,spa103_render.sourceChannels);
  assert(seg[i].front>0);
 }
 /* the AAC (stereo) segments are upmixed by the scene at level AGGRESSIVE only; APAC 5.1 passes through */
 assert(nv_opens==1&&nv_closes==0&&!spa_nv_shim_faulted()&&spa_nv_shim.faults==0);
 assert(!spa103_render.decodeFailed&&!spa103_render.authFailed&&!spa103_render.tlvFailed&&!spa103_render.upmixFailed);
 drain(16);
 unsigned long long in=spa103_render.framesIn,out=nv_frames;size_t left=spa_buffered_pcm_pending(6);
 printf("frames: in=%llu native_out=%llu ring_left=%u formats switched\n",in,out,(unsigned)left);
 assert(in==out+left);
 spa_pcm_sink_close_detached("switch-test");spa103_render_stop();
 printf("NVIDIA hook live APAC<->AAC switching PASS (%u native writes, one connection)\n",nwr);
 return 0;
}

/* SCENE mask net on the async worker vs the synchronous reference path, plus worker lifecycle. */
static int run_mask_scenario(unsigned *seqp){
 const char *blob=getenv("AUDIO_SCENE_MASK_BLOB");assert(blob&&*blob);
 unsigned long long surr[2],front[2];struct spa_scene_mask_worker_stats st[2];unsigned writes[2],started[2];
 (void)spa_audio_config();
 for(unsigned run=0;run<2;run++){
  /* a63d547: the synchronous mask path is gone from the hook (AUDIO_SCENE_MASK_SYNC removed); both runs use the worker */
  set_level_file("2");(void)spa_upmix_level_reload();
  spa103_render_start(400+run,65,1,21168000,0,0,0);assert(spa103_render_set_rate(400+run,1));
  apac_idx=0;feed_apac(6,seqp);
  assert(spa_pcm_sink_bootstrap_buffered()==1);
  aac_seq=*seqp;aac_left=0;
  unsigned base=nwr;feed_aac(30);
  if(run){memset(&st[1],0,sizeof(st[1]));}
  if(run)(void)spa_stereo_upmix_scene_track_change(&spa103_render.stereoUpmix); /* generation/stale isolation mid-stream */
  feed_aac(30);*seqp=aac_seq;
  writes[run]=nwr-base;front[run]=window_sum(wr_front,base,nwr);surr[run]=window_sum(wr_surr,base+20,nwr);
  started[run]=(unsigned)spa103_render.sceneMaskWorkerStarted;
  if(spa103_render.sceneMaskWorkerStarted)spa_scene_mask_worker_get_stats(&spa103_render.sceneMaskWorker,&st[run]);
  else memset(&st[run],0,sizeof(st[run]));
  printf("mask %s: writes=%u front=%llu surround=%llu worker_started=%u submitted=%u completed=%u busy=%u stale=%u superseded=%u resets=%u\n",
   run?"async-2":"async-1",writes[run],front[run],surr[run],started[run],st[run].submitted,st[run].completed,st[run].busy,st[run].stale_discarded,st[run].superseded,st[run].resets);
  spa_pcm_sink_close_detached("mask-run");spa103_render_stop();
 }
 assert(started[0]&&started[1]&&st[0].submitted>20&&st[1].submitted>20&&st[1].completed>10);
 double ratio=(double)surr[1]/(double)(surr[0]?surr[0]:1);
 printf("mask async/sync surround energy ratio=%.3f\n",ratio);
 assert(surr[0]>0&&ratio>0.5&&ratio<2.0); /* two worker runs of the same stream must agree */
 /* lifecycle: worker thread start/stop must not hang or leak across stream restarts */
 for(unsigned i=0;i<20;i++){spa103_render_start(500+i,65,1,21168000,0,0,0);spa103_render_stop();}
 printf("NVIDIA hook SCENE mask worker: async vs sync, stale isolation, 20 start/stop cycles PASS\n");
 return 0;
}

/* AAC-LC/44.1k (index 22) resampled to 48k inside the bridge, through the NVIDIA hook path. */
static const unsigned char aac_cookie44[]={3,0x80,0x80,0x80,0x22,0,0,0,4,0x80,0x80,0x80,0x14,0x40,0x14,0,0x18,0,0,1,0xf4,0,0,1,0xf4,0,5,0x80,0x80,0x80,2,0x12,0x10,6,0x80,0x80,0x80,1,2};
static double crossings_hz(const int16_t *x,unsigned from,unsigned to){unsigned c=0;for(unsigned i=from+1;i<to;i++)if((x[i-1]<0)!=(x[i]<0))++c;return c/2.0/((double)(to-from)/48000.0);}
static int run_rate44_scenario(const unsigned char *cookie,size_t cookie_size,size_t lengths,unsigned *seqp){
 const char *fix=getenv("NV_FIX44");assert(fix&&*fix);
 size_t sz;unsigned char *blob=read_blob(fix,&sz);unsigned count=(unsigned)blob[8]|((unsigned)blob[9]<<8)|((unsigned)blob[10]<<16)|((unsigned)blob[11]<<24);
 spa103_render_start(400,65,1,21168000,0,0,0);assert(spa103_render_set_rate(400,1));
 apac_idx=0;apac_lengths=lengths;apac_cookie=cookie;apac_cookie_size=cookie_size;feed_apac(6,seqp);
 assert(spa_pcm_sink_bootstrap_buffered()==1);
 unsigned long long in0=spa103_render.framesIn;cap_on=1;cap_n=0;size_t at=12;unsigned fed=0;
 for(unsigned i=0;i<count;i++){
  unsigned n=(unsigned)blob[at]|((unsigned)blob[at+1]<<8)|((unsigned)blob[at+2]<<16)|((unsigned)blob[at+3]<<24);at+=4;
  feed_idx(22,22,(*seqp)++,aac_cookie44,sizeof(aac_cookie44),blob+at,n);at+=n;++fed;
  for(unsigned k=0;k<4&&spa_buffered_pcm_pending(6)>=768;k++)spa_pcm_sink_write_one_detached_period();
 }
 drain(16);cap_on=0;
 double per_au=(double)(spa103_render.framesIn-in0)/(double)fed;
 printf("rate44: format=%d channels=%u AUs=%u frames/AU=%.3f decodeFailed=%llu native_frames=%u\n",(int)spa103_render.formatIndex,spa103_render.sourceChannels,fed,per_au,(unsigned long long)spa103_render.decodeFailed,cap_n);
 assert((spa103_render.formatIndex==22||spa103_render.formatIndex==23)&&!spa103_render.decodeFailed&&!spa103_render.upmixFailed);
 assert(per_au>1113.9&&per_au<1115.1);
 unsigned from=6144+4000,to=cap_n>8000?cap_n-4000:cap_n;assert(to>from+20000);
 double fl=crossings_hz(cap_l,from,to),fr=crossings_hz(cap_r,from,to);
 double rms=0;for(unsigned i=from;i<to;i++)rms+=(double)cap_l[i]*(double)cap_l[i];rms=sqrt(rms/(to-from));
 printf("rate44 tones: L=%.1f Hz R=%.1f Hz rmsL=%.0f (expect 1000/3000, ~1448: ffmpeg sine 0.125 x 0.5)\n",fl,fr,rms);
 assert(fl>990&&fl<1010&&fr>2970&&fr<3030&&rms>1330&&rms<1570);
 spa_pcm_sink_close_detached("rate44");spa103_render_stop();free(blob);
 printf("NVIDIA hook AAC-LC 44.1k fallback: bridge resample to 48k PASS\n");
 return 0;
}

#ifdef MHI2_NV_STOCK_BRIDGE
/* Stock libairplay-style client (open / set_pcm_info / create_metadata / write / close) through the bridge. */
#if defined(MHI2_PIPELINE_NATIVE)
#define STOCK_CHECK(x) ((void)0) /* counters/captures are host-mock only; the guest checks return codes and logs */
#else
#define STOCK_CHECK(x) assert(x)
#endif
static int run_stock_scenario(const unsigned char *cookie,size_t cookie_size,size_t lengths,unsigned *seqp){
 static int16_t st[768*2];void *h=0;unsigned meta[2],info[3]={48000,2,2};const unsigned char sad[20]={0,2,0,1,16,16};
 assert(!setenv("AUDIO_REALTIME_UPMIX","on",1));set_level_file("0");(void)spa_upmix_level_reload();
 /* 1. pass-through: the navigation format on the claimed device (real open, untouched) and, on the host mock, another device */
 {void *o=0;unsigned nav[3]={16000,2,1};
#ifndef MHI2_PIPELINE_NATIVE
  {void *x=0;assert(!nvaudio_pcm_open(&x,"MPL9"));assert(!nvaudio_pcm_close(x));assert(nv_opens==1&&nv_closes==1);nv_opens=nv_closes=0;}
#endif
  {int rc=nvaudio_pcm_open(&o,SPA_NV_STOCK_DEVICE);assert(!rc);}assert(!nvaudio_set_pcm_info(o,nav));assert(!nvaudio_pcm_close(o));
  STOCK_CHECK(nv_opens==1&&nv_closes==1&&!spa_nv_shim.sinkStarted);printf("stock: navigation format passes through to the real device (opens=%u)\n",nv_opens);}
 nv_opens=nv_closes=0;
 /* the stock AudioStreamPrepare declares a media main stream before the device is opened */
 assert(spa_stock_main_audio_begin((void *)0x1234,SPA_DIO_AUDIO_TYPE_MEDIA,"prepare")==1);
 /* 2. stock music on the claimed device, upmix OFF: L/R from the stock buffer, rest zero, native 6ch */
 assert(!nvaudio_pcm_open(&h,SPA_NV_STOCK_DEVICE));assert(!nvaudio_set_pcm_info(h,info));assert(!nvaudio_create_metadata(h,sad,meta));
 STOCK_CHECK(nv_opens==1&&!strcmp(nv_device,"MPL1")); /* shim device comes from MHI2_NV_DEVICE in this test */
 unsigned n0=nwr;cap_on=1;cap_n=0;
 for(unsigned n=0;n<40;n++){for(unsigned f=0;f<768;f++){st[f*2]=(int16_t)(1000+n);st[f*2+1]=(int16_t)-(1000+n);}assert(!nvaudio_pcm_write(h,st,sizeof(st),meta,20));}
 cap_on=0;
 printf("stock: music writes=%u native_frames=%u opens=%u\n",nwr-n0,cap_n,nv_opens);
 STOCK_CHECK(nv_opens==1&&cap_n>=39*768&&cap_n<=41*768);
 unsigned okfr=0;for(unsigned i=0;i<cap_n;i++)if(cap_l[i]==-cap_r[i]&&cap_l[i]>=1000&&cap_l[i]<1040)okfr++;
 printf("stock: %u of %u native frames carry the stock L/R\n",okfr,cap_n);STOCK_CHECK(okfr>=cap_n*9/10);
 STOCK_CHECK(window_sum(wr_surr,n0,nwr)==0); /* OFF: bypass, C/LFE/Ls/Rs digital zero */
 /* 3. realtime upmix level 4 on the same live stream */
 spa_upmix_level=SPA_UPMIX_LEVEL_AGGRESSIVE;
 {unsigned b=nwr;static int16_t tone[768*2];
  for(unsigned n=0;n<60;n++){for(unsigned f=0;f<768;f++){double t=(double)(n*768+f)/48000.0;tone[f*2]=(int16_t)(8000*sin(6.2831853*440*t));tone[f*2+1]=(int16_t)(8000*sin(6.2831853*660*t+0.7));}assert(!nvaudio_pcm_write(h,tone,sizeof(tone),meta,20));}
  printf("stock: upmix level 4 surround energy=%llu over %u writes\n",window_sum(wr_surr,b+20,nwr),nwr-b);STOCK_CHECK(window_sum(wr_surr,b+20,nwr)>0);}
 /* 4. format change on the same handle: navigation 16k mono leaves the claim, music comes back */
 {unsigned nav[3]={16000,2,1};unsigned before=nv_opens;assert(!nvaudio_set_pcm_info(h,nav));assert(!nvaudio_set_pcm_info(h,info));
  printf("stock: 48k -> 16k mono -> 48k opens %u->%u closes=%u\n",before,nv_opens,nv_closes);}
/* 5. type-103 starts while stock type-100 owns the handle: ring audio replaces the stock fallback, ONE connection */
 {unsigned o=nv_opens,c=nv_closes;apac_idx=0;apac_lengths=lengths;apac_cookie=cookie;apac_cookie_size=cookie_size;
  spa103_render_start(400,65,1,21168000,0,0,0);assert(spa103_render_set_rate(400,1));feed_apac(6,seqp);
  assert(spa_buffered_pcm_pending(6)>=768);
  static int16_t marker[768*2];for(unsigned f=0;f<768;f++){marker[f*2]=12345;marker[f*2+1]=-12345;}
  cap_on=1;cap_n=0;unsigned pend=(unsigned)spa_buffered_pcm_pending(6);
  for(unsigned n=0;n<4;n++)assert(!nvaudio_pcm_write(h,marker,sizeof(marker),meta,20));cap_on=0;
  unsigned stockLeft=0;for(unsigned i=0;i<cap_n;i++)if(cap_l[i]==12345&&cap_r[i]==-12345)stockLeft++;
  printf("stock+103: stock frames in native out=%u of %u, ring %u->%u, opens=%u closes=%u\n",stockLeft,cap_n,pend,(unsigned)spa_buffered_pcm_pending(6),nv_opens,nv_closes);
  STOCK_CHECK(nv_opens==o&&nv_closes==c);STOCK_CHECK(stockLeft<cap_n/4);assert(spa_buffered_pcm_pending(6)<pend);
  /* stock closes (focus lost): the handle stays alive for the buffered receiver, no reopen */
  spa_stock_main_audio_end((void *)0x1234,"stop_done"); /* AudioStreamStop completes before the device closes */
  assert(!nvaudio_pcm_close(h));h=0;STOCK_CHECK(nv_closes==c);
  printf("state after stock close: detached=%d handle=%p expanded=%d params=%d closeFlushSuppressed=%d needsPrepare=%d pending=%u hook_owned=%d\n",spa_pcm_sink.detached,(void*)spa_pcm_sink.handle,spa_pcm_sink.expandedParams,(int)spa_pcm_sink.paramsState,spa_pcm_sink.closeFlushSuppressed,spa_pcm_sink.detachedNeedsPrepare,(unsigned)spa_buffered_pcm_pending(6),spa_pcm_sink.route.hook_owned);
  usleep(120000); /* the route is re-claimed only after the stock release has had time to queue (50 ms) */
  {unsigned w=nv_writes;feed_apac(6,seqp);drain(16);printf("stock+103: after stock close detached writes %u->%u opens=%u closes=%u\n",w,nv_writes,nv_opens,nv_closes);
   STOCK_CHECK(nv_writes>w&&nv_opens==o&&nv_closes==c);}
  spa_pcm_sink_close_detached("stock-test");STOCK_CHECK(nv_closes==c+1);spa103_render_stop();}
 if(h){spa_stock_main_audio_end((void *)0x1234,"stop_done");assert(!nvaudio_pcm_close(h));}
 STOCK_CHECK(nv_opens==nv_closes);
 printf("NVIDIA stock bridge: claim 48k stereo, pass-through others, type-100 upmix PASS\n");
 (void)cookie;(void)cookie_size;(void)lengths;(void)seqp;
 return 0;
}
#endif
#if defined(MHI2_NV_STOCK_BRIDGE)&&defined(MHI2_NV_BRIDGE_LOCAL_MEDIA)
/* mm-renderer-like client (Bluetooth/USB/SD on MPL1): no AudioStreamPrepare, 44.1 kHz stereo. The bridge must declare
 * the media route itself, resample to 48 kHz, upmix at level 4 and release the route on close. */
static int run_local_scenario(void){
 static int16_t st[1024*2];void *h=0;unsigned meta[2],info[3]={44100,2,2};const unsigned char sad[20]={0,2,0,1,16,16};
 assert(!setenv("AUDIO_REALTIME_UPMIX","on",1));set_level_file("0");(void)spa_upmix_level_reload();
 {void *o=0;unsigned nav[3]={16000,2,1};assert(!nvaudio_pcm_open(&o,SPA_NV_STOCK_DEVICE));assert(!nvaudio_set_pcm_info(o,nav));assert(!nvaudio_pcm_close(o));
  STOCK_CHECK(!spa_media_route_has_media(&spa_pcm_sink.route));printf("local: navigation format passes through, no media route\n");}
 assert(!spa_media_route_has_media(&spa_pcm_sink.route));
 assert(!nvaudio_pcm_open(&h,SPA_NV_STOCK_DEVICE));assert(!nvaudio_set_pcm_info(h,info));assert(!nvaudio_create_metadata(h,sad,meta));
 assert(spa_media_route_has_media(&spa_pcm_sink.route));
 unsigned n0=nwr;cap_on=1;cap_n=0;
 for(unsigned n=0;n<40;n++){for(unsigned f=0;f<1024;f++){st[f*2]=1000;st[f*2+1]=-1000;}assert(!nvaudio_pcm_write(h,st,sizeof(st),meta,20));}
 cap_on=0;
 unsigned want=40u*1024u*160u/147u,okfr=0;for(unsigned i=0;i<cap_n;i++)if(cap_l[i]==-cap_r[i]&&cap_l[i]>=998&&cap_l[i]<=1002)okfr++;
 printf("local: 44.1k in %u frames -> native %u frames (expected ~%u), %u carry L/R, surround=%llu\n",40u*1024u,cap_n,want,okfr,window_sum(wr_surr,n0,nwr));
 STOCK_CHECK(cap_n+2048>=want&&cap_n<=want+2048);STOCK_CHECK(okfr>=cap_n*8/10);STOCK_CHECK(window_sum(wr_surr,n0,nwr)==0);
 spa_upmix_level=SPA_UPMIX_LEVEL_AGGRESSIVE;
 {unsigned b=nwr;static int16_t tone[1024*2];
  for(unsigned n=0;n<60;n++){for(unsigned f=0;f<1024;f++){double t=(double)(n*1024+f)/44100.0;tone[f*2]=(int16_t)(8000*sin(6.2831853*440*t));tone[f*2+1]=(int16_t)(8000*sin(6.2831853*660*t+0.7));}assert(!nvaudio_pcm_write(h,tone,sizeof(tone),meta,20));}
  printf("local: upmix level 4 surround energy=%llu over %u writes\n",window_sum(wr_surr,b+20,nwr),nwr-b);STOCK_CHECK(window_sum(wr_surr,b+20,nwr)>0);}
 assert(!nvaudio_pcm_close(h));
 assert(!spa_media_route_has_media(&spa_pcm_sink.route));
 printf("NVIDIA local-media bridge: 44.1k claim, resample, own media route, upmix, release PASS\n");
 return 0;
}
/* Randomised stress: arbitrary write sizes (never multiples of the period), 44.1k/48k/16k format flips on a live
 * handle, pause = close and reopen, level changes and sink failures injected at random writes. Invariants: no crash or
 * assert, the media route is held only while a claim is open, every open is closed, and the claimed 44.1k stream makes
 * progress (native frames within the resampler ratio of what was fed while no failure was injected). */
static unsigned long long rng_state=88172645463325252ull;
static unsigned rnd(unsigned n){rng_state^=rng_state<<13;rng_state^=rng_state>>7;rng_state^=rng_state<<17;return (unsigned)(rng_state%n);}
static int run_local_stress(void){
 static int16_t buf[2048*2];void *h=0;unsigned meta[2];const unsigned char sad[20]={0,2,0,1,16,16};
 unsigned fmt[3][3]={{44100,2,2},{48000,2,2},{16000,2,1}};unsigned cur=0,opens=0,closes=0,fed=0;unsigned long long fedFrames44=0,natAtStart=0;
 assert(!setenv("AUDIO_REALTIME_UPMIX","on",1));set_level_file("0");(void)spa_upmix_level_reload();
 for(unsigned it=0;it<4000;it++){
  unsigned op=rnd(100);
  if(!h){assert(!nvaudio_pcm_open(&h,SPA_NV_STOCK_DEVICE));++opens;cur=rnd(3);assert(!nvaudio_set_pcm_info(h,fmt[cur]));assert(!nvaudio_create_metadata(h,sad,meta));natAtStart=cap_n;fedFrames44=0;continue;}
  if(op<70){unsigned frames=1+rnd(2048);for(unsigned f=0;f<frames;f++){int v=(int)(8000*sin(6.2831853*(220.0+cur*110.0)*(double)(fed+f)/44100.0));buf[f*2]=(int16_t)v;buf[f*2+1]=(int16_t)(v/2);}
   fed+=frames;if(cur==0)fedFrames44+=frames;
   if(rnd(40)==0)nv_fail_at=nv_writes_total+1+rnd(3); /* sink failure on one of the next writes */
   nv_passthrough=cur==2;int rc=nvaudio_pcm_write(h,buf,frames*(cur==2?2:4),meta,20);nv_passthrough=0;(void)rc; /* a stall may report an error; it must never corrupt state */
   continue;}
  if(op<80){unsigned n=rnd(3);if(n!=cur||rnd(2)){cur=n;assert(!nvaudio_set_pcm_info(h,fmt[cur]));}continue;}
  if(op<88){spa_upmix_level=rnd(5);continue;}
  assert(!nvaudio_pcm_close(h));h=0;++closes;
  assert(!spa_media_route_has_media(&spa_pcm_sink.route));
 }
 if(h){assert(!nvaudio_pcm_close(h));++closes;}
 assert(!spa_media_route_has_media(&spa_pcm_sink.route));
 printf("local stress: %u opens, %u closes, %u frames fed, native writes %u\n",opens,closes,fed,nv_writes);
 assert(opens==closes&&opens>20&&nv_writes>200);(void)natAtStart;(void)fedFrames44;
 printf("NVIDIA local-media bridge stress: random sizes/format flips/pauses/failures, route balanced PASS\n");
 return 0;
}
#endif
int main(int argc,char **argv){
 int stock_mode=getenv("MHI2_NV_SCENARIO")&&!strcmp(getenv("MHI2_NV_SCENARIO"),"stock");
 int r44_mode=getenv("MHI2_NV_SCENARIO")&&!strcmp(getenv("MHI2_NV_SCENARIO"),"rate44");
 int mask_mode=getenv("MHI2_NV_SCENARIO")&&!strcmp(getenv("MHI2_NV_SCENARIO"),"mask");
 int sw_mode=getenv("MHI2_NV_SCENARIO")&&!strcmp(getenv("MHI2_NV_SCENARIO"),"switch");
 int sc_mode=getenv("MHI2_NV_SCENARIO")&&!strcmp(getenv("MHI2_NV_SCENARIO"),"soundcheck");
 int level_mode=getenv("MHI2_NV_SCENARIO")&&!strcmp(getenv("MHI2_NV_SCENARIO"),"level");
 set_level_file("0"); /* never inherit a level left by an earlier scenario */
 if(getenv("MHI2_NV_SCENARIO")&&(!strcmp(getenv("MHI2_NV_SCENARIO"),"stock")||!strcmp(getenv("MHI2_NV_SCENARIO"),"local")))assert(!setenv("AUDIO_REALTIME_UPMIX","on",1));
 assert(argc==3);(void)argv[2];assert(!setenv("AUDIO_PRIME_MS","20",1));
 assert(!setenv("AUDIO_SPATIAL","1",1));assert(!setenv("AUDIO_STEREO_UPMIX",(level_mode||sw_mode||mask_mode||stock_mode||(getenv("MHI2_NV_SCENARIO")&&!strcmp(getenv("MHI2_NV_SCENARIO"),"local")))?(getenv("NV_UPMIX")?getenv("NV_UPMIX"):"on"):"off",1));assert(!setenv("MHI2_NV_DEVICE","MPL1",1));
 struct mhi2_nv_api api={nv_open,nv_config,nv_meta,nv_write,nv_close};
#if !(defined(MHI2_PIPELINE_NATIVE)&&defined(MHI2_NV_STOCK_BRIDGE))
 spa_nv_shim_set_api(&api); /* native stock-bridge run uses the real libnvaudio via RTLD_NEXT instead */
#endif
 assert(!spa_apac_codec_get_api(1,&real_apac_api,sizeof(real_apac_api)));test_runtime_codec=&real_apac_api;
 g_blob=read_blob(argv[1],&g_blobBytes);assert(rd32(8)==6&&rd32(12)==6&&rd32(16)==1024);
 size_t cookie_size=rd32(20);const unsigned char *cookie=g_blob+24;size_t lengths=24+cookie_size,at=lengths+24;
 memcpy(spa103_receiver.shk,kTestKey,sizeof(kTestKey));
 spa103_render_start(400,65,1,21168000,0,0,0);assert(spa103_render_set_rate(400,1));
 unsigned seq=1;
 for(unsigned i=0;i<6;i++){unsigned n=rd32(lengths+i*4);feed(seq++,cookie,cookie_size,g_blob+at,n);at+=n;}
 if(stock_mode){
#ifdef MHI2_NV_STOCK_BRIDGE
  spa103_render_stop();int rc=run_stock_scenario(cookie,cookie_size,lengths,&seq);free(g_blob);return rc;
#else
  return 77;
#endif
 }
 if(getenv("MHI2_NV_SCENARIO")&&!strcmp(getenv("MHI2_NV_SCENARIO"),"local")){
#if defined(MHI2_NV_STOCK_BRIDGE)&&defined(MHI2_NV_BRIDGE_LOCAL_MEDIA)
  spa103_render_stop();int rc=run_local_scenario();if(!rc&&getenv("MHI2_NV_STRESS"))rc=run_local_stress();free(g_blob);return rc;
#else
  return 77;
#endif
 }
 if(r44_mode){spa103_render_stop();int rc=run_rate44_scenario(cookie,cookie_size,lengths,&seq);free(g_blob);return rc;}
 if(mask_mode){size_t sz;aac_blob=read_blob(argv[2],&sz);aac_bytes=sz;aac_count=(unsigned)aac_blob[8]|((unsigned)aac_blob[9]<<8)|((unsigned)aac_blob[10]<<16)|((unsigned)aac_blob[11]<<24);apac_lengths=lengths;apac_cookie=cookie;apac_cookie_size=cookie_size;spa103_render_stop();int rc=run_mask_scenario(&seq);free(aac_blob);free(g_blob);return rc;}
 if(sw_mode){size_t sz;aac_blob=read_blob(argv[2],&sz);aac_bytes=sz;aac_count=(unsigned)aac_blob[8]|((unsigned)aac_blob[9]<<8)|((unsigned)aac_blob[10]<<16)|((unsigned)aac_blob[11]<<24);apac_lengths=lengths;apac_cookie=cookie;apac_cookie_size=cookie_size;apac_idx=6;set_level_file("4");(void)spa_upmix_level_reload();int rc=run_switch_scenario(&seq);free(aac_blob);free(g_blob);return rc;}
 if(sc_mode){size_t sz;aac_blob=read_blob(argv[2],&sz);aac_bytes=sz;aac_count=(unsigned)aac_blob[8]|((unsigned)aac_blob[9]<<8)|((unsigned)aac_blob[10]<<16)|((unsigned)aac_blob[11]<<24);spa103_render_stop();int rc=run_soundcheck_scenario(cookie,cookie_size,lengths,&seq);free(aac_blob);free(g_blob);return rc;}
 if(level_mode){size_t sz;aac_blob=read_blob(argv[2],&sz);aac_bytes=sz;aac_count=(unsigned)aac_blob[8]|((unsigned)aac_blob[9]<<8)|((unsigned)aac_blob[10]<<16)|((unsigned)aac_blob[11]<<24);aac_seq=seq;(void)aac_bytes;set_level_file("0");int rc=run_level_scenario();free(aac_blob);free(g_blob);return rc;}
 size_t pending=spa_buffered_pcm_pending(6);printf("pending after feed=%u\n",(unsigned)pending);assert(pending>=1024);
 if(getenv("NV_DUMP"))nv_dump=fopen(getenv("NV_DUMP"),"wb");
 tdm_tag(100);
 assert(spa_pcm_sink_bootstrap_buffered()==1);
 assert(nv_opens==1&&nv_cfg==1&&!strcmp(nv_device,"MPL1")&&spa_nv_shim.generation==1);
 /* nominal: periods flow through the shim until the ring runs short */
 drain(16);assert(nv_writes>=1);unsigned first_writes=nv_writes;
 printf("nominal: opens=%u native_writes=%u frames=%u silent=%u\n",nv_opens,nv_writes,nv_frames,silent_writes);
 /* refill, then inject one native failure: period dropped, reopen, silence preroll, no replay */
 at=lengths+24;unsigned before=nv_writes_total;nsums=0;/* same AUs again: hashes only unique within one phase */
 for(unsigned i=0;i<6;i++){unsigned n=rd32(lengths+i*4);feed(seq++,cookie,cookie_size,g_blob+at,n);at+=n;}
 size_t pend_before=spa_buffered_pcm_pending(6);
 tdm_tag(101);nv_fail_at=before+2;drain(1);
 size_t pend_mid=spa_buffered_pcm_pending(6);
 drain(2);
 printf("recovery: opens=%u closes=%u gen=%u faults=%llu pending %u->%u silent=%u writes=%u\n",nv_opens,nv_closes,spa_nv_shim.generation,spa_nv_shim.faults,(unsigned)pend_before,(unsigned)pend_mid,silent_writes,nv_writes);
 drain(16);
 assert(spa_nv_shim.faults>=1&&nv_opens==2&&nv_closes==1&&spa_nv_shim.generation>=2&&silent_writes>=1&&nv_writes>first_writes);
 /* Route down: stock closed MPL5 on pause, so every write fails for a while (write status 4 on the unit).
  * Failures stay bounded (no reopen storm), nothing replays, and writes resume once the route is back. */
 unsigned route_down_extra=0;
 {unsigned o=nv_opens,c=nv_closes,w=nv_writes;unsigned long long fl=spa_nv_shim.faults;
  at=lengths+24;nsums=0;for(unsigned i=0;i<6;i++){unsigned n=rd32(lengths+i*4);feed(seq++,cookie,cookie_size,g_blob+at,n);at+=n;}
  nv_fail_at=nv_writes_total+1;nv_fail_count=6;
  drain(24);
  printf("route down: opens %u->%u closes %u->%u faults %llu->%llu\n",o,nv_opens,c,nv_closes,fl,spa_nv_shim.faults);
  assert(spa_nv_shim.faults>fl);
  assert(nv_opens-o<=nv_fail_count+1);
  assert(nv_opens-o==nv_closes-c);
  nv_fail_at=0;nv_fail_count=0;
  at=lengths+24;nsums=0;for(unsigned i=0;i<6;i++){unsigned n=rd32(lengths+i*4);feed(seq++,cookie,cookie_size,g_blob+at,n);at+=n;}
  drain(16);assert(nv_writes>w);
  route_down_extra=nv_opens-o;}
 /* FLUSHBUFFERED-style timeline flush: native reopen, new generation, writes continue */
 tdm_tag(102);
 {unsigned o=nv_opens,w=nv_writes,g=spa_nv_shim.generation;
  dump_rec(3,0,0);
  assert(spa_pcm_sink_flush_buffered_timeline(spa103_render.generation)==1);
  assert(nv_opens==o+1&&spa_nv_shim.generation!=g&&spa_pcm_sink.timelineFlushes==1&&!spa_pcm_sink.timelineFlushFailures);
  at=lengths+24;nsums=0;for(unsigned i=0;i<6;i++){unsigned n=rd32(lengths+i*4);feed(seq++,cookie,cookie_size,g_blob+at,n);at+=n;}
  drain(16);assert(nv_writes>w);printf("flush: opens %u->%u gen %u->%u writes %u->%u\n",o,nv_opens,g,spa_nv_shim.generation,w,nv_writes);}
 spa_pcm_sink_close_detached("test");
#ifdef MHI2_PIPELINE_NATIVE
 usleep(400000);
#endif
 tdm_tag(0);if(nv_dump)fclose(nv_dump);assert(nv_closes==3+route_down_extra&&nv_opens==3+route_down_extra);spa103_render_stop();free(g_blob);
 printf("NVIDIA hook detached path: shim write/recover PASS (%u native writes, %u frames, %u silent)\n",nv_writes,nv_frames,silent_writes);
 return 0;
}
