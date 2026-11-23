/* Host execution of the identical synthetic probe; no native-device claim. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
int nvaudio_pcm_open(void **h,const char *name){(void)name;*h=(void *)1;return 0;}
int nvaudio_set_pcm_info(void *h,const void *p){(void)h;const unsigned *c=p;return c[0]!=48000||c[1]!=2||c[2]!=6;}
int nvaudio_create_metadata(void *h,const void *p,unsigned *m){(void)h;(void)p;m[0]=m[1]=0;return 0;}
static FILE *capture;
int nvaudio_pcm_write(void *h,const void *p,unsigned n,const unsigned *m,unsigned t){
 (void)h;(void)m;(void)t;
 const char *path=getenv("MHI2_HOST_PCM");
 if(path&&!capture)capture=fopen(path,"wb");
 if(path&&(!capture||fwrite(p,1,n,capture)!=n))return 1;
 return n%12!=0;
}
int nvaudio_pcm_close(void *h){(void)h;if(capture){fclose(capture);capture=NULL;}return 0;}
int usleep(unsigned t){(void)t;return 0;}

extern void native_scene_entry(void);
int main(void){native_scene_entry();return 1;}
