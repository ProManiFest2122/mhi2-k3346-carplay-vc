/* Simulates only the stock stereo PCM client's public nvaudio ABI. */
typedef unsigned u32;
extern int nvaudio_pcm_open(void **,const char *),nvaudio_set_pcm_info(void *,const void *);
extern int nvaudio_create_metadata(void *,const void *,u32 *),nvaudio_pcm_write(void *,const void *,u32,const u32 *,u32);
extern int nvaudio_pcm_close(void *),usleep(u32),write(int,const void *,u32),snprintf(char *,u32,const char *,...);
extern int open(const char *,int,...),close(int);
extern void *mmap64(void *,u32,int,int,int,unsigned long long);
extern void exit(int);
static short pcm[512*2];
void _start(void){
 void *h=0;u32 cfg[3]={48000,2,2},meta[2]={0};unsigned char sad[20]={0,192,0,1,16,16};*(u32 *)(sad+8)=48000;
 int fd=open("/dev/mem",2);void *m=fd>=0?mmap64(0,4096,0x700,1,fd,0x70080000ull):(void *)-1;if(fd>=0)close(fd);if(m==(void *)-1)exit(1);
 volatile u32 *marker=(volatile u32 *)((char *)m+0xe20);
 if(nvaudio_pcm_open(&h,"MPL1")||nvaudio_set_pcm_info(h,cfg)||nvaudio_create_metadata(h,sad,meta))exit(2);
 *marker=64;
 unsigned errors=0;
 for(unsigned b=0;b<192;b++){
  for(unsigned i=0;i<512;i++){unsigned t=b*512+i;pcm[i*2]=(short)(((int)(t%97)-48)*61);pcm[i*2+1]=(short)(((int)(t%137)-68)*47);}
  errors+=nvaudio_pcm_write(h,pcm,sizeof(pcm),meta,250)!=0;
 }
 nvaudio_pcm_close(h);*marker=0;
 char text[128];int n=snprintf(text,sizeof(text),"STEREO_CLIENT writes=192 errors=%u\n",errors);write(1,text,n);exit(errors?3:0);
}
