/* Host tool: render a 48 kHz stereo S16 WAV through the reference SCENE upmixer (768-frame periods, like the realtime
 * path) into a 6-channel WAV (L R C LFE Ls Rs).  usage: scene-render-wav <level 0-4> <mask 0|1> in.wav out.wav */
#include "stereo_upmix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
extern const unsigned char full_scene_mask[],full_scene_mask_end[];
#define PERIOD 768u
static struct spa_stereo_upmix state;
static void le32(unsigned char *p,uint32_t v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
static void le16(unsigned char *p,uint16_t v){p[0]=v;p[1]=v>>8;}
int main(int argc,char **argv){
 if(argc!=5)return 2;
 unsigned level=(unsigned)atoi(argv[1]);int mask=atoi(argv[2]);
 FILE *f=fopen(argv[3],"rb");if(!f)return 3;
 unsigned char h[44];if(fread(h,1,44,f)!=44||memcmp(h,"RIFF",4)||memcmp(h+36,"data",4)||h[22]!=2||h[24]!=0x80||h[25]!=0xbb)return 4; /* 48000, 2ch, canonical header */
 uint32_t bytes=h[40]|(h[41]<<8)|(h[42]<<16)|((uint32_t)h[43]<<24);size_t frames=bytes/4;
 int16_t *in=malloc(bytes);if(fread(in,1,bytes,f)!=bytes)return 5;fclose(f);
 struct spa_stereo_upmix_config cfg;spa_stereo_upmix_default_config(&cfg);cfg.backend=SPA_STEREO_UPMIX_SCENE;cfg.level=level;
 if(mask)spa_stereo_upmix_config_set_scene_mask_blob(&cfg,full_scene_mask,(size_t)(full_scene_mask_end-full_scene_mask));
 if(spa_stereo_upmix_init(&state,&cfg))return 6;
 size_t padded=(frames+PERIOD-1)/PERIOD*PERIOD;int16_t *out=calloc(padded+PERIOD*4,6*sizeof(int16_t));int16_t *src=calloc(padded,4);memcpy(src,in,bytes);
 for(size_t at=0;at<padded;at+=PERIOD)if(spa_stereo_upmix_process_s16(&state,src+at*2,PERIOD,out+at*6,PERIOD))return 7;
 unsigned char o[44]={'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,6,0,0x80,0xbb,0,0,0,0,0,0,12,0,16,0,'d','a','t','a'};
 le32(o+4,36+padded*12);le32(o+28,48000*12);le16(o+32,12);le32(o+40,padded*12);
 f=fopen(argv[4],"wb");fwrite(o,1,44,f);fwrite(out,12,padded,f);fclose(f);return 0;}
