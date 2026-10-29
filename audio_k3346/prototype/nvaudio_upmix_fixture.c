/* Emulator-only ABI fixture. Single serialized synthetic client. No focus,
 * concurrency or real AirPlay validation: do not preload on a vehicle. */
#include "mhi2_scene_adapter.h"
#ifndef FIXTURE_DEVICE
#define FIXTURE_DEVICE "MPL5"
#endif
extern void *dlopen(const char *,int), *dlsym(void *,const char *);
extern int snprintf(char *,size_t,const char *,...);
extern int strcmp(const char *,const char *);
extern int write(int,const void *,size_t);
static struct mhi2_scene_adapter adapter;
static short output[MHI2_AUDIO_MAX_FRAMES*6];
static void *selected;
static unsigned active;
static int write_failure;
#ifdef MHI2_AUDIO_SETTINGS
extern int open(const char *,int,...),read(int,void *,size_t),close(int);
static unsigned configured_level;
static unsigned read_level(void){
 unsigned char b[3];int fd=open("/tmp/mhi2_audio_level",0);if(fd<0)return 0;
 int n=read(fd,b,sizeof(b));close(fd);
 if(n!=2||b[0]<'0'||b[0]>'4'||b[1]!=10)return 0;
 return b[0]-'0';
}
#endif
static unsigned metadata[2];
static unsigned energy[6],calls;
static int (*real_open)(void **,const char *);
static int (*real_config)(void *,const void *);
static int (*real_write)(void *,const void *,unsigned,const unsigned *,unsigned);
static int (*real_close)(void *);
static int (*real_metadata)(void *,const void *,unsigned *);
static int resolve(void) {
 if(real_open)return 0;
 void *lib=dlopen("libnvaudio.so",1);if(!lib)return -1;
 real_open=dlsym(lib,"nvaudio_pcm_open");real_config=dlsym(lib,"nvaudio_set_pcm_info");
 real_write=dlsym(lib,"nvaudio_pcm_write");real_close=dlsym(lib,"nvaudio_pcm_close");
 real_metadata=dlsym(lib,"nvaudio_create_metadata");
 return !(real_open&&real_config&&real_write&&real_close&&real_metadata);
}
int nvaudio_pcm_open(void **h,const char *name) {
 if(resolve())return 1;
 int r=real_open(h,name);
 if(!r&&!selected&&name&&!strcmp(name,FIXTURE_DEVICE)){
#ifdef MHI2_AUDIO_SETTINGS
  selected=*h;active=0;write_failure=0;calls=0;for(unsigned i=0;i<6;i++)energy[i]=0;
#else
  if(!mhi2_scene_adapter_init(&adapter)){selected=*h;active=0;write_failure=0;calls=0;for(unsigned i=0;i<6;i++)energy[i]=0;}
#endif
 }
 return r;
}
int nvaudio_set_pcm_info(void *h,const void *p) {
 if(resolve())return 1;
 const unsigned *c=p;
#ifdef MHI2_AUDIO_SETTINGS
 if(h==selected){
  configured_level=read_level();
  if(!configured_level){int r=real_config(h,p);if(!r){active=0;write_failure=0;mhi2_scene_adapter_mode(&adapter,0,0);}return r;}
 }
#endif
 if(h!=selected||!c||c[0]!=48000||c[1]!=2||c[2]!=2){
  int r=real_config(h,p);if(!r&&h==selected){active=0;write_failure=0;mhi2_scene_adapter_reset(&adapter);}return r;
 }
 if(!adapter.initialized&&mhi2_scene_adapter_init(&adapter))return 1;
 unsigned six[3]={48000,2,6};
 int r=real_config(h,six);if(r)return r;
 unsigned char sad[20]={0,252,0,1,16,16};*(unsigned *)(sad+8)=48000;
 r=real_metadata(h,sad,metadata);
 if(r){real_config(h,p);active=0;write_failure=0;mhi2_scene_adapter_reset(&adapter);return r;}
 mhi2_scene_adapter_mode(&adapter,1,0);mhi2_scene_adapter_reset(&adapter);active=1;write_failure=0;
#ifdef MHI2_AUDIO_SETTINGS
 mhi2_scene_adapter_level(&adapter,configured_level);
#endif
 const char text[]="NVAUDIO_UPMIX_FIXTURE ARMED\n";write(2,text,sizeof(text)-1);
 return 0;
}
int nvaudio_pcm_write(void *h,const void *p,unsigned bytes,const unsigned *meta,unsigned timeout) {
 if(resolve())return 1;
 if(h!=selected||!active)return real_write(h,p,bytes,meta,timeout);
 if(write_failure)return write_failure;
 if(!p&&bytes)return 7;
 if(bytes%4||bytes/4>MHI2_AUDIO_MAX_FRAMES)return 7;
 unsigned frames=bytes/4;if(!frames)return 0;
 if(mhi2_scene_adapter_process(&adapter,p,frames,output))return 7;
 for(unsigned i=0;i<frames*6;i++)energy[i%6]+=(unsigned)(output[i]<0?-output[i]:output[i]);
 calls++;
 int status=real_write(h,output,frames*12,metadata,timeout);
 /* The native error does not prove zero bytes consumed. Do not advance DSP
  * again on a retry; explicit successful reconfigure/close is required. */
 if(status)write_failure=status;
 return status;
}
int nvaudio_pcm_close(void *h) {
 if(resolve())return 1;
 if(h==selected){char text[256];int n=snprintf(text,sizeof(text),"UPMIX_FIXTURE_STATS calls=%u meta=%u energy=%u,%u,%u,%u,%u,%u\n",calls,metadata[0],energy[0],energy[1],energy[2],energy[3],energy[4],energy[5]);write(2,text,n);}
 int r=real_close(h);if(!r&&h==selected){selected=0;active=0;write_failure=0;mhi2_scene_adapter_reset(&adapter);}
 return r;
}

#ifdef MHI2_EXPERIMENTAL_AIRPLAY_CONFIGURE
/* Exact K3346/MU1438 ABI only; static layout verified against live SHA-256.
 * Still single-owner fixture: no concurrency/focus deployment claim.
 * configure returns C++ bool (1 success), unlike nvaudio's zero success. */
extern void *memcpy(void *,const void *,size_t);
typedef int (*airplay_configure_fn)(void *,unsigned,unsigned,unsigned);
int _ZN14CAudioDriverIF9configureEjjj(void *self,unsigned rate,unsigned bits,unsigned channels)
{
 static airplay_configure_fn original;
 if(!original){void *lib=dlopen("libairplay.so",1);if(lib)original=(airplay_configure_fn)dlsym(lib,"_ZN14CAudioDriverIF9configureEjjj");}
 if(!original||!self)return 0;
 int ok=original(self,rate,bits,channels);
 if(!ok)return ok;
 void *handle=0;memcpy(&handle,(const char *)self+4,sizeof(handle));
 if(handle!=selected)return ok;
#ifdef MHI2_AUDIO_SETTINGS
 /* Off on a fresh stream must not introduce a lower PCM reconfiguration. */
 if(!active&&!read_level())return ok;
#endif
 if(bits%8||!bits)return ok;
 /* Stock skips the lower call if rate already matches. Apply the observed
  * source format here even at 48k; the lower wrapper selects music only. */
 unsigned info[3]={rate,bits/8,channels};
 return nvaudio_set_pcm_info(handle,info)==0;
}
#endif
