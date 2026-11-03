/* Exact reference SCENE, mask net and five live levels. Local QNX/host probe only. */
#include "stereo_upmix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
extern const unsigned char full_scene_mask[],full_scene_mask_end[];
static struct spa_stereo_upmix state;
static short input[1024*2],output[1024*6];
static unsigned long long energy[6];
static unsigned samples;
static void run(void){
 struct spa_stereo_upmix_config cfg;spa_stereo_upmix_default_config(&cfg);
 cfg.backend=SPA_STEREO_UPMIX_SCENE;cfg.level=SPA_UPMIX_LEVEL_NORMAL;
 /* Keep the reference B&O profile confined to this numerical probe. */
 spa_stereo_upmix_config_set_scene_mask_blob(&cfg,full_scene_mask,(size_t)(full_scene_mask_end-full_scene_mask));
 if(!cfg.scene_mask_blob||spa_stereo_upmix_init(&state,&cfg))exit(1);
 for(unsigned block=0;block<96;block++){
  if(block==16)spa_stereo_upmix_set_level(&state,SPA_UPMIX_LEVEL_OFF);
  if(block==32)spa_stereo_upmix_set_level(&state,SPA_UPMIX_LEVEL_MODERATE);
  if(block==48)spa_stereo_upmix_set_level(&state,SPA_UPMIX_LEVEL_NORMAL);
  if(block==64)spa_stereo_upmix_set_level(&state,SPA_UPMIX_LEVEL_FULL);
  if(block==80)spa_stereo_upmix_set_level(&state,SPA_UPMIX_LEVEL_AGGRESSIVE);
  for(unsigned i=0;i<1024;i++){unsigned n=block*1024+i;input[2*i]=(short)(((int)(n%197)-98)*97);input[2*i+1]=(short)(((int)(n%337)-168)*53);}
  if(spa_stereo_upmix_process_s16(&state,input,1024,output,1024))exit(2);
  for(unsigned i=0;i<1024;i++)for(unsigned c=0;c<6;c++){int v=output[6*i+c];energy[c]+=v<0?-v:v;if(c==2&&v)exit(3);if(c==3&&v)exit(4);}
  samples+=1024;
 }
 char text[256];int n=snprintf(text,sizeof(text),"FULL_SCENE_NATIVE frames=%u mask=1 live_levels=5 energy=%llu,%llu,%llu,%llu,%llu,%llu\n",samples,energy[0],energy[1],energy[2],energy[3],energy[4],energy[5]);write(1,text,n);
 if(!energy[0]||!energy[1]||!energy[4]||!energy[5])exit(5);
 if(spa_stereo_upmix_reset(&state))exit(6);
 memset(input,0,sizeof(input));
 for(unsigned b=0;b<8;b++){if(spa_stereo_upmix_process_s16(&state,input,1024,output,1024))exit(7);for(unsigned i=0;i<1024*6;i++)if(output[i])exit(8);}
 {const char done[]="FULL_SCENE_NATIVE reset_silence=1 exit=0\n";write(1,done,sizeof(done)-1);}exit(0);
}
#ifdef MHI2_HOST
int main(void){run();return 0;}
#else
void _start(void){run();}
#endif
