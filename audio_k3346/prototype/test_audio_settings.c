#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
int fixture_open(void **,const char *);int fixture_config(void *,const void *);
int fixture_write(void *,const void *,unsigned,const unsigned *,unsigned);int fixture_close(void *);
int _ZN14CAudioDriverIF9configureEjjj(void *,unsigned,unsigned,unsigned);
static int token,level;static unsigned configs,writes;static unsigned format[3];static const void *last;
int fixture_file_open(const char *s,int flags,...){(void)flags;assert(!strcmp(s,"/tmp/mhi2_audio_level"));return level<0?-1:10;}
int fixture_file_read(int fd,void *p,size_t n){assert(fd==10&&n==3);char *b=p;b[0]=(char)('0'+level);b[1]=10;return 2;}
int fixture_file_close(int fd){assert(fd==10);return 0;}
static int open_real(void **h,const char *s){(void)s;*h=&token;return 0;}
static int config_real(void *h,const void *p){(void)h;configs++;memcpy(format,p,sizeof(format));return 0;}
static int metadata_real(void *h,const void *p,unsigned *m){(void)h;(void)p;m[0]=0x3fc00;m[1]=0;return 0;}
static int write_real(void *h,const void *p,unsigned n,const unsigned *m,unsigned t){(void)h;(void)m;(void)t;last=p;writes++;if(format[2]==6){assert(n%12==0);const short *x=p;for(unsigned i=0;i<n/12;i++)assert(!x[i*6+2]&&!x[i*6+3]);}return 0;}
static int close_real(void *h){(void)h;return 0;}
static int airplay_real(void *p,unsigned r,unsigned b,unsigned c){(void)p;(void)r;(void)b;(void)c;return 1;}
void *fixture_dlopen(const char *s,int f){(void)s;(void)f;return &token;}
void *fixture_dlsym(void *h,const char *s){(void)h;
 if(!strcmp(s,"nvaudio_pcm_open"))return (void *)open_real;
 if(!strcmp(s,"nvaudio_set_pcm_info"))return (void *)config_real;
 if(!strcmp(s,"nvaudio_pcm_write"))return (void *)write_real;
 if(!strcmp(s,"nvaudio_pcm_close"))return (void *)close_real;
 if(!strcmp(s,"nvaudio_create_metadata"))return (void *)metadata_real;
 if(!strcmp(s,"_ZN14CAudioDriverIF9configureEjjj"))return (void *)airplay_real;
 return 0;
}
static short pcm[512*2];
int main(void){
 void *h;unsigned char object[4+sizeof(void *)];unsigned meta[2]={0};
 for(int v=-1;v<=5;v++){
  level=v;assert(!fixture_open(&h,"MPL1"));memcpy(object+4,&h,sizeof(h));
  configs=0;format[2]=2;
  assert(_ZN14CAudioDriverIF9configureEjjj(object,48000,16,2));
  if(v<1||v>4){assert(!configs);assert(!fixture_write(h,pcm,sizeof(pcm),meta,30));assert(last==pcm);}
  else {assert(configs==1&&format[2]==6);for(unsigned b=0;b<8;b++){for(unsigned i=0;i<512*2;i++)pcm[i]=(short)((i%97)*17);assert(!fixture_write(h,pcm,sizeof(pcm),meta,30));assert(last!=pcm);}}
  assert(!fixture_close(h));
 }
 puts("PASS: missing/Off/invalid setting leaves stock config untouched; levels1–4 active; C/LFEzero");
}
