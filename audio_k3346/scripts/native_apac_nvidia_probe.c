/* Emulator-only: dlopen the real PIC codec -> discrete PCM6 -> native NVIDIA.
 * Route eligibility is true ONLY for the previously qualified emulator MPL1. */
#include "nvidia_pcm_sink.h"
#include "apac_bridge.h"
#include "spatial_downmix.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
extern const unsigned char apac_fixture[],apac_fixture_end[];
extern int nvaudio_pcm_open(void **,const char *),nvaudio_set_pcm_info(void *,const void *),nvaudio_create_metadata(void *,const void *,unsigned *),nvaudio_pcm_write(void *,const void *,unsigned,const unsigned *,unsigned),nvaudio_pcm_close(void *);
static struct mhi2_nv_sink sink;
static short decoded[16*1024],declared[6*1024];
static unsigned rd(unsigned at){const unsigned char *p=apac_fixture+at;return (unsigned)p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);}
static void report(const char *key,unsigned a,unsigned b){char s[160];int n=snprintf(s,sizeof(s),"APAC_NVIDIA_NATIVE %s=%u detail=%u\n",key,a,b);write(1,s,n);}
void _start(void){
 int fd=open("/dev/mem",O_RDWR);void *map=fd>=0?mmap64(0,4096,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0x70080000ull):(void *)-1;
 if(fd>=0)close(fd);if(map==(void *)-1)exit(20);
 volatile unsigned *marker=(volatile unsigned *)((char *)map+0xe20);*marker=96;
 void *lib=dlopen("/lib/libmhi2_apac.so",RTLD_NOW);if(!lib){report("loader_error",1,0);exit(1);}
 int (*getapi)(uint32_t,spa_apac_codec_api_t *,size_t)=(void *)dlsym(lib,"spa_apac_codec_get_api");spa_apac_codec_api_t codec;
 if(!getapi||getapi(1,&codec,sizeof(codec))||codec.structSize!=sizeof(codec)||codec.abiVersion!=1){report("api_error",1,0);exit(2);}
 unsigned packets=rd(8),channels=rd(12),length=rd(16),cookiebytes=rd(20);
 if(packets!=6||channels!=6||length!=1024)exit(3);
 spa_apac_decoder_t *decoder=0;uint32_t nch=0,frames=0,rate=0;
 if(codec.create(apac_fixture+24,cookiebytes,&decoder,&nch,&frames,&rate)||nch!=6||frames!=1024||rate!=48000)exit(4);
 struct mhi2_nv_api api={nvaudio_pcm_open,nvaudio_set_pcm_info,nvaudio_create_metadata,nvaudio_pcm_write,nvaudio_pcm_close};
 int status=mhi2_nv_sink_init(&sink,&api,"MPL1",1);if(status)exit(5);
 status=mhi2_nv_sink_start(&sink);report("open_config",status,sink.metadata[1]);if(status)exit(6);
 unsigned generation=mhi2_nv_sink_generation(&sink),lengths=24+cookiebytes,at=lengths+packets*4,total=0;
 for(unsigned i=0;i<packets;i++){
  unsigned n=rd(lengths+i*4);if(at+n>(unsigned)(apac_fixture_end-apac_fixture))exit(7);
  if(codec.decodeS16(decoder,apac_fixture+at,n,decoded,16*1024,&frames)||frames!=1024)exit(8);
  at+=n;spa_reorder_5_1_decoder_to_declared_s16(decoded,frames,declared);
  struct mhi2_nv_result result=mhi2_nv_sink_write(&sink,generation,declared,frames);
  report("packet_status",result.status,(unsigned)result.confirmed_frames);
  if(result.status||result.confirmed_frames!=frames||result.uncertain_frames)exit(9);total+=frames;
 }
 usleep(200000);*marker=0;
 status=mhi2_nv_sink_flush(&sink);report("flush",status,mhi2_nv_sink_generation(&sink));if(status)exit(10);
 struct mhi2_nv_result stale=mhi2_nv_sink_write(&sink,generation,declared,512);
 if(stale.status!=MHI2_NV_STALE)exit(11);
 generation=mhi2_nv_sink_generation(&sink);
 /* A successful close/reopen does not prove the downstream ring is empty.
  * Keep its settling interval outside the independently marked tones. */
 memset(declared,0,sizeof(declared));
 for(unsigned block=0;block<64;block++){
  struct mhi2_nv_result silence=mhi2_nv_sink_write(&sink,generation,declared,512);
  if(silence.status||silence.confirmed_frames!=512||silence.uncertain_frames)exit(14);
 }
 usleep(200000);
 for(unsigned ch=0;ch<6;ch++){
  *marker=16+ch;memset(declared,0,sizeof(declared));
  for(unsigned f=0;f<512;f++)declared[f*6+ch]=(short)((ch+1)*1000);
  for(unsigned block=0;block<64;block++){
   struct mhi2_nv_result tone=mhi2_nv_sink_write(&sink,generation,declared,512);
   if(tone.status||tone.confirmed_frames!=512||tone.uncertain_frames)exit(13);
  }
  usleep(200000);*marker=0;
  report("onehot_channel",ch,64);
 }
 usleep(200000);*marker=0;
 status=mhi2_nv_sink_destroy(&sink);report("close",status,total);if(status)exit(12);
 codec.destroy(decoder);dlclose(lib);report("PASS",packets,total);exit(0);
}
