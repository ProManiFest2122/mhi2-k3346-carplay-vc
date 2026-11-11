/* ARM/QNX differential decode of the pinned public six-packet fixture. */
#include "apac/config.h"
#include "apac/decoder.h"
#include <stddef.h>
extern "C" {extern const unsigned char apac_fixture[],apac_fixture_end[];int snprintf(char *,size_t,const char *,...);int write(int,const void *,size_t);void exit(int);}
static apac::GlobalConfig config;
static apac::ASPDecoder decoder;
static float pcm[16*1024];
static unsigned rd(unsigned p){return unsigned(apac_fixture[p])|(unsigned(apac_fixture[p+1])<<8)|(unsigned(apac_fixture[p+2])<<16)|(unsigned(apac_fixture[p+3])<<24);}
static unsigned be(const unsigned char *p){return (unsigned(p[0])<<24)|(unsigned(p[1])<<16)|(unsigned(p[2])<<8)|p[3];}
extern "C" void _start(void){
 unsigned packets=rd(8),channels=rd(12),length=rd(16),cookiebytes=rd(20);
 if(packets!=6||channels!=6||length!=1024)exit(1);
 const unsigned char *cookie=apac_fixture+24;unsigned off=0,bytes=0;
 for(unsigned p=0;p+8<=cookiebytes;){unsigned n=be(cookie+p),type=be(cookie+p+4);if(n<8||p+n>cookiebytes)break;if(type==0x64617061){off=p+12;bytes=n-12;break;}p+=(type==0x66726d61||type==0x61706163)?8:n;}
 if(!off||bytes<=2)exit(2);
 config.Reset();apac::BitstreamReader bits(cookie+off+2,bytes-2);
 if(config.Deserialize(bits)||decoder.Initialize(config)||decoder.NumChannels()!=6)exit(3);
 unsigned lengths=24+cookiebytes,packetat=lengths+packets*4,pcmat=packetat;
 for(unsigned i=0;i<packets;i++)pcmat+=rd(lengths+i*4);
 if(pcmat+packets*channels*length*4!=unsigned(apac_fixture_end-apac_fixture))exit(4);
 float peak=0,worst=0;unsigned invalid=0,total=0;
 for(unsigned i=0;i<packets;i++){
  unsigned n=rd(lengths+i*4);uint32_t frames=0;
  if(decoder.DecodeFrame(apac_fixture+packetat,n,pcm,&frames)||frames!=length)exit(5);packetat+=n;
  for(unsigned k=0;k<channels*length;k++){
   union {unsigned u;float f;} want;want.u=rd(pcmat+(i*channels*length+k)*4);
   float a=want.f<0?-want.f:want.f,d=pcm[k]-want.f;if(d<0)d=-d;
   if(a>peak)peak=a;if(d>worst)worst=d;invalid+=pcm[k]!=pcm[k];total++;
  }
 }
 char text[256];unsigned ratio=peak>0?(unsigned)(worst/peak*1000000000.0f):0;
 int n=snprintf(text,sizeof(text),"APAC_NATIVE packets=%u channels=%u samples=%u invalid=%u max_relative_error_ppb=%u\n",packets,channels,total,invalid,ratio);write(1,text,n);
 uint32_t frames=123;unsigned status=decoder.DecodeFrame(0,0,pcm,&frames);
 n=snprintf(text,sizeof(text),"APAC_NATIVE_FLUSH status=%u frames=%u\n",status,frames);write(1,text,n);
 exit(invalid||ratio>5000||status||frames?6:0);
}
