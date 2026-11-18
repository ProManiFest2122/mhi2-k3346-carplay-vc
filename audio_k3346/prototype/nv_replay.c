/* Control experiment: replay the host-recorded native write stream (watermarked) through the
 * NVIDIA sink at device rate, with no codec/hook CPU load.  Same capture/analyzer as the hook run.
 * Record types: 0 audio, 1 silence, 2 injected failed write (not forwarded; reopen follows), 3 timeline flush. */
#include "nvidia_pcm_sink.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
extern int nvaudio_pcm_open(void **,const char *),nvaudio_set_pcm_info(void *,const void *),nvaudio_create_metadata(void *,const void *,unsigned *),nvaudio_pcm_write(void *,const void *,unsigned,const unsigned *,unsigned),nvaudio_pcm_close(void *);
static volatile unsigned *marker;
static void tag(unsigned v){if(!marker){int fd=open("/dev/mem",2);void *m=fd>=0?mmap64(0,4096,0x700,1,fd,0x70080000ull):(void *)-1;if(fd>=0)close(fd);if(m==(void *)-1){printf("marker map failed\n");return;}marker=(volatile unsigned *)((char *)m+0xe20);}*marker=v;}
int main(int argc,char **argv){
 if(argc<2)return 2;
 if(getenv("NV_RAMP")){/* clean control: ramp on L/R, C=write#, LFE=frame# , 512-frame writes at device rate */
  struct mhi2_nv_api api={nvaudio_pcm_open,nvaudio_set_pcm_info,nvaudio_create_metadata,nvaudio_pcm_write,nvaudio_pcm_close};
  struct mhi2_nv_sink sink;static int16_t buf[512*6];
  if(mhi2_nv_sink_init(&sink,&api,"MPL1",1)||mhi2_nv_sink_start(&sink))return 4;
  unsigned gen=mhi2_nv_sink_generation(&sink),g=0;tag(100);
  for(unsigned w=1;w<=60;w++){for(unsigned fr=0;fr<512;fr++,g++){int16_t *q=buf+fr*6;q[0]=q[1]=(int16_t)(100+(g%9000));q[2]=(int16_t)w;q[3]=(int16_t)(fr+1);q[4]=q[5]=0;}
   struct mhi2_nv_result r=mhi2_nv_sink_write(&sink,gen,buf,512);if(r.status){printf("ramp: write failed %d\n",r.status);return 8;}
   if(getenv("NV_PACE"))usleep(10666);}
  usleep(400000);tag(0);mhi2_nv_sink_destroy(&sink);printf("NVIDIA replay ramp: 60 writes PASS\n");return 0;}
 FILE *f=fopen(argv[1],"rb");if(!f){printf("replay: cannot open %s\n",argv[1]);return 3;}
 int pace=getenv("NV_PACE")!=0;
 struct mhi2_nv_api api={nvaudio_pcm_open,nvaudio_set_pcm_info,nvaudio_create_metadata,nvaudio_pcm_write,nvaudio_pcm_close};
 struct mhi2_nv_sink sink;static int16_t pcm[1024*6];
 if(mhi2_nv_sink_init(&sink,&api,"MPL1",1)||mhi2_nv_sink_start(&sink)){printf("replay: sink start failed\n");return 4;}
 unsigned type,frames,seq=0,gen=mhi2_nv_sink_generation(&sink),writes=0,flushes=0;
 tag(100);
 while(fread(&type,4,1,f)==1&&fread(&frames,4,1,f)==1){
  ++seq;
  if(type==2){tag(101);if(mhi2_nv_sink_flush(&sink)){printf("replay: reopen failed\n");return 5;}gen=mhi2_nv_sink_generation(&sink);++flushes;continue;}
  if(type==3){tag(102);if(mhi2_nv_sink_flush(&sink)){printf("replay: flush failed\n");return 6;}gen=mhi2_nv_sink_generation(&sink);++flushes;--seq;continue;}
  if(frames>1024||fread(pcm,12,frames,f)!=frames){printf("replay: bad record\n");return 7;}
  struct mhi2_nv_result r=mhi2_nv_sink_write(&sink,gen,pcm,frames);
  if(r.status||r.confirmed_frames!=frames){printf("replay: write failed seq=%u status=%d\n",seq,r.status);return 8;}
  ++writes;
  if(pace)usleep((unsigned)((unsigned long long)frames*1000000ull/48000ull));
 }
 usleep(400000);tag(0);
 mhi2_nv_sink_destroy(&sink);fclose(f);
 printf("NVIDIA replay: %u writes, %u reopen/flush PASS (pace=%d)\n",writes,flushes,pace);
 return 0;
}
