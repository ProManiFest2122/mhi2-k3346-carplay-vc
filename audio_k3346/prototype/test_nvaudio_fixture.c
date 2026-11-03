/* Host ABI mock: preserve live 16k mono prompts, isolate other handles,
 * reject unsupported music block sizes, latch ambiguous native write errors. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
int fixture_open(void **,const char *);int fixture_config(void *,const void *);
int fixture_write(void *,const void *,unsigned,const unsigned *,unsigned);int fixture_close(void *);
int _ZN14CAudioDriverIF9configureEjjj(void *,unsigned,unsigned,unsigned);
static int airplay_ok=1;
static int airplay_real(void *self,unsigned rate,unsigned bits,unsigned channels){(void)self;(void)rate;(void)bits;(void)channels;return airplay_ok;}
static int token;static unsigned config[3],last_bytes,writes;static const void *last_input;static int fail;
static int open_real(void **h,const char *name){(void)name;*h=&token;return 0;}
static int config_real(void *h,const void *p){(void)h;memcpy(config,p,sizeof(config));return 0;}
static int metadata_real(void *h,const void *p,unsigned *m){(void)h;(void)p;m[0]=0x3fc00;m[1]=0;return 0;}
static int write_real(void *h,const void *p,unsigned n,const unsigned *m,unsigned t){(void)h;(void)m;(void)t;last_input=p;last_bytes=n;writes++;return fail;}
static int close_real(void *h){(void)h;return 0;}
void *fixture_dlopen(const char *s,int f){(void)s;(void)f;return &token;}
void *fixture_dlsym(void *h,const char *s){(void)h;
 if(!strcmp(s,"_ZN14CAudioDriverIF9configureEjjj"))return (void *)airplay_real;
 if(!strcmp(s,"nvaudio_pcm_open"))return (void *)open_real;
 if(!strcmp(s,"nvaudio_set_pcm_info"))return (void *)config_real;
 if(!strcmp(s,"nvaudio_pcm_write"))return (void *)write_real;
 if(!strcmp(s,"nvaudio_pcm_close"))return (void *)close_real;
 if(!strcmp(s,"nvaudio_create_metadata"))return (void *)metadata_real;
 return 0;
}
static short pcm[1025*6];
int main(void){
 void *h=0;unsigned music[]={48000,2,2},prompt[]={16000,2,1},spatial[]={48000,2,6},meta[]={0,0};
 assert(!fixture_open(&h,"MPL1"));assert(!fixture_config(h,music));assert(config[2]==6);
 assert(!fixture_write(h,pcm,512*4,meta,30));assert(last_bytes==512*12&&last_input!=pcm);
 assert(!fixture_config(h,prompt));assert(config[0]==16000&&config[2]==1);
 assert(!fixture_write(h,pcm,512*2,meta,30));assert(last_input==pcm&&last_bytes==512*2);
 assert(!fixture_config(h,spatial));assert(!fixture_write(h,pcm,512*12,meta,30));assert(last_input==pcm);
 assert(!fixture_config(h,music));unsigned before=writes;
 assert(fixture_write(h,pcm,1025*4,meta,30)==7&&writes==before);
 assert(fixture_write(h,0,512*4,meta,30)==7&&writes==before);
 fail=9;assert(fixture_write(h,pcm,512*4,meta,30)==9);before=writes;
 fail=0;assert(fixture_write(h,pcm,512*4,meta,30)==9&&writes==before);
 assert(!fixture_config(h,music));assert(!fixture_write(h,pcm,512*4,meta,30));
 int other;assert(!fixture_write(&other,pcm,1024,meta,30)&&last_input==pcm);
 unsigned char object[4+sizeof(void *)];memcpy(object+4,&h,sizeof(h));
 assert(_ZN14CAudioDriverIF9configureEjjj(object,16000,16,1)==1&&config[0]==16000&&config[2]==1);
 assert(_ZN14CAudioDriverIF9configureEjjj(object,48000,16,2)==1&&config[2]==6);
 airplay_ok=0;before=writes;assert(!_ZN14CAudioDriverIF9configureEjjj(object,16000,16,1)&&config[2]==6&&writes==before);airplay_ok=1;
 assert(!fixture_close(h));assert(!fixture_open(&h,"ANN1"));assert(!fixture_config(h,music));assert(config[2]==2);assert(!fixture_close(h));
 puts("PASS: 48k stereo upmix; 16k mono/native 6ch unchanged; other handles isolated; invalid packets rejected; write fault latched; AirPlay same-rate configuration and bool semantics");
}
