/* Emulator-only: write the TDM capture marker (as native_airplay_stream_probe does) from /tmp/mhi2_tdm_marker
 * (decimal, 0..999). Lets shell-driven runs (nvaudio_play) label capture blocks. No phone, no vehicle. */
#include <stddef.h>
extern int open(const char *,int,...),read(int,void *,size_t),close(int);extern void *mmap64(void *,size_t,int,int,int,unsigned long long);
extern void exit(int);
void _start(void){
 char c[8]={0};unsigned v=0;int fd=open("/tmp/mhi2_tdm_marker",0),n,i;
 if(fd<0)exit(2);n=read(fd,c,7);close(fd);
 for(i=0;i<n&&c[i]>='0'&&c[i]<='9';i++)v=v*10+(unsigned)(c[i]-'0');
 fd=open("/dev/mem",2);void *m=fd>=0?mmap64(0,4096,0x300,1,fd,0x70080000ull):(void *)-1;if(fd>=0)close(fd);
 if(m==(void *)-1)exit(5);
 *(volatile unsigned *)((char *)m+0xe20)=v;exit(0);
}
