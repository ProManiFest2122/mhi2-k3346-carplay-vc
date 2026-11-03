/* Emulator-only: K3346 CAudioDriverIF ctor/open/configure/write bracketed by AudioStreamPrepare/Stop on a fake
 * main-stream object (type 100 at +108, audioType 1 = media at +112). Run ONLY with the hook preloaded plus
 * libmhi2_test_stream_stub.so (so real libairplay never sees the fake object). No phone, no vehicle. */
#include <stddef.h>
extern void driver_ctor(void *,const char *) __asm__("_ZN14CAudioDriverIFC1EPKc");
extern int driver_open(void *) __asm__("_ZN14CAudioDriverIF4openEv");
extern int driver_configure(void *,unsigned,unsigned,unsigned) __asm__("_ZN14CAudioDriverIF9configureEjjj");
extern int driver_write(void *,const unsigned char *,unsigned,unsigned) __asm__("_ZN14CAudioDriverIF5writeEPKhjj");
extern void driver_close(void *) __asm__("_ZN14CAudioDriverIF5closeEv");
extern int AudioStreamPrepare(void *);extern void AudioStreamStop(void *,unsigned char);
extern int open(const char *,int,...),read(int,void *,size_t),close(int);extern void *mmap64(void *,size_t,int,int,int,unsigned long long);extern int usleep(unsigned);
extern int snprintf(char *,size_t,const char *,...),write(int,const void *,size_t);extern void exit(int);
static unsigned long long object[32];static unsigned char stream[256] __attribute__((aligned(8)));static short pcm[512*2];
static void report(const char *name,int status){char text[128];int n=snprintf(text,sizeof(text),"AIRPLAY_DRIVER_NATIVE %s=%d\n",name,status);write(1,text,n);}
static volatile unsigned *tdm_marker(void){int fd=open("/dev/mem",2);void *m=fd>=0?mmap64(0,4096,0x300,1,fd,0x70080000ull):(void *)-1;if(fd>=0)close(fd);return m==(void *)-1?0:(volatile unsigned *)((char *)m+0xe20);}
static unsigned level_from_file(void){char c[8]={0};int fd=open("/tmp/mhi2_audio_level",0);if(fd<0)return 0;int n=read(fd,c,7);close(fd);return n>0&&c[0]>='0'&&c[0]<='9'?(unsigned)(c[0]-'0'):0;}
void _start(void){
 volatile unsigned *marker=tdm_marker();unsigned level=level_from_file();report("level",level);if(!marker)exit(5);*marker=200+level;
 *(int *)(stream+108)=100;*(int *)(stream+112)=1;
 int prep=AudioStreamPrepare(stream);report("prepare",prep);if(prep)exit(4);
 driver_ctor(object,"MPL1");int status=driver_open(object);report("open",status);if(!status)exit(1);
 status=driver_configure(object,48000,16,2);report("configure_same_rate",status);if(!status)exit(2);
 unsigned errors=0;
 for(unsigned b=0;b<32;b++){for(unsigned i=0;i<512;i++){unsigned t=b*512+i;pcm[i*2]=(short)(((int)(t%97)-48)*61);pcm[i*2+1]=(short)(((int)(t%137)-68)*47);}errors+=driver_write(object,(const unsigned char *)pcm,sizeof(pcm),250)!=0;}
 report("write_errors",errors);
 driver_close(object);AudioStreamStop(stream,1);report("stop",0);usleep(300000);*marker=0;unsigned cleared=((unsigned *)object)[1]==0;report("close_handle_cleared",cleared);exit(errors||!cleared?3:0);
}
