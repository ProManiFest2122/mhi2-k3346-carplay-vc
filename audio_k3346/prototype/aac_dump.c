/* Decode the raw AAC fixture through the K3346 AAC bridge and print every PCM sample as hex lines
 * (each chunk twice with a CRC) so host and guest output can be diffed.  Research tool only. */
#include "apac_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
extern const spa_apac_codec_api_t *spa_aac_codec_api(void);
static const unsigned char cookie[]={3,0x80,0x80,0x80,0x22,0,0,0,4,0x80,0x80,0x80,0x14,0x40,0x14,0,0x18,0,0,1,0xf4,0,0,1,0xf4,0,5,0x80,0x80,0x80,2,0x11,0x90,6,0x80,0x80,0x80,1,2};
static unsigned crc(const unsigned char *p,size_t n){unsigned c=0xffffffffu;for(size_t i=0;i<n;i++){c^=p[i];for(int k=0;k<8;k++)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;}
int main(int argc,char **argv){
 if(argc<2)return 2;
 FILE *f=fopen(argv[1],"rb");if(!f)return 3;fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
 unsigned char *b=malloc((size_t)n);if(fread(b,1,(size_t)n,f)!=(size_t)n)return 4;fclose(f);
 unsigned count=(unsigned)b[8]|((unsigned)b[9]<<8)|((unsigned)b[10]<<16)|((unsigned)b[11]<<24);
 const spa_apac_codec_api_t *api=spa_aac_codec_api();spa_apac_decoder_t *d=0;uint32_t ch,fr,rate;
 if(api->create(cookie,sizeof(cookie),&d,&ch,&fr,&rate))return 5;
 size_t at=12;static int16_t pcm[2048];unsigned long long total=0;
 for(unsigned au=0;au<count;au++){
  unsigned len=(unsigned)b[at]|((unsigned)b[at+1]<<8)|((unsigned)b[at+2]<<16)|((unsigned)b[at+3]<<24);at+=4;
  uint32_t frames=0;if(api->decodeS16(d,b+at,len,pcm,2048,&frames))return 6;at+=len;
  for(unsigned off=0;off<frames*2;off+=64){
   unsigned m=frames*2-off<64?frames*2-off:64;char line[600];int p=sprintf(line,"S %u %u ",au,off);
   for(unsigned i=0;i<m;i++)p+=sprintf(line+p,"%04x",(unsigned)(uint16_t)pcm[off+i]);
   unsigned c=crc((const unsigned char *)line,(size_t)p);
   for(int rep=0;rep<2;rep++)printf("%s %08x\n",line,c);
  }
  total+=frames;
 }
 printf("AACDUMP_DONE frames=%llu\n",total);
 return 0;
}
