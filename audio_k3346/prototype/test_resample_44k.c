#include "resample_44k_to_48k.h"
#include <assert.h>
#include <stdio.h>
static struct rs44_state st;
static int16_t in[RS_MAX_IN*2],out[RS_MAX_OUT*2];
/* tone at f Hz, amplitude 0.5, sampled at rate; chunked arbitrarily */
static size_t run(double f,size_t chunk,size_t total_in,int16_t *dst,size_t cap_frames){
 rs44_reset(&st);size_t produced=0,fed=0;
 while(fed<total_in){size_t n=total_in-fed<chunk?total_in-fed:chunk;
  for(size_t i=0;i<n;i++){double v=0.5*sin(2*3.14159265358979323846*f*(double)(fed+i)/44100.0);in[i*2]=(int16_t)lrint(v*32767);in[i*2+1]=(int16_t)lrint(-v*32767);}
  size_t o=rs44_process(&st,in,n,out);assert(produced+o<=cap_frames);memcpy(dst+produced*2,out,o*4);produced+=o;fed+=n;}
 return produced;}
static int16_t a[200000*2],b[200000*2];
int main(void){
 /* output frame count: 147 AUs of 1024 -> 163840 minus the half-kernel latency frames not yet emitted */
 size_t n=run(1000.0,1024,147*1024,a,200000);
 printf("frames out for 147 AUs: %zu (ideal 163840, latency %u in-frames)\n",n,(unsigned)RS_HALF);
 assert(n>163840-30&&n<=163840);
 unsigned c1114=0,c1115=0;rs44_reset(&st);
 for(unsigned au=0;au<294;au++){memset(in,0,sizeof(in));size_t o=rs44_process(&st,in,1024,out);if(o==1114)c1114++;else if(o==1115)c1115++;else assert(au==0&&o>=1090);}
 printf("per-AU outputs: 1114 x%u, 1115 x%u\n",c1114,c1115);assert(c1114>=120&&c1115>=160);
 /* chunk independence: same result for 1024 vs 333 vs 1 frame chunks (bit exact) */
 size_t n2=run(1000.0,333,147*1024,b,200000);assert(n2==n&&!memcmp(a,b,n*4));
 /* accuracy: compare with the ideal 48 kHz tone after the filter delay, several tones */
 double fs[4]={100.0,1000.0,8000.0,18000.0};
 for(int k=0;k<4;k++){size_t m=run(fs[k],1024,20*1024,a,200000);double err=0,sig=0;
  /* output frame j corresponds to input time (j*147/160 - 16... ) ; the filter is symmetric so group delay is 0 on the grid: test with measured best lag */
  double best=1e30;for(int lag=-40;lag<=40;lag++){double e=0,s=0;for(size_t j=4000;j<m-4000;j++){double t=((double)j+lag/10.0)*44100.0/48000.0-0.0;double ideal=0.5*sin(2*3.14159265358979323846*fs[k]*t/44100.0);
     double got=a[j*2]/32767.0;e+=(got-ideal)*(got-ideal);s+=ideal*ideal;}if(e<best){best=e;sig=s;}}
  err=best;double db=10*log10(err/sig);printf("tone %.0f Hz: residual %.1f dB\n",fs[k],db);assert(db<(fs[k]>16000.0?-40.0:-60.0));}
 /* DC and level: unity gain */
 rs44_reset(&st);for(unsigned i=0;i<1024;i++){in[i*2]=in[i*2+1]=16000;}
 size_t o=0;for(int r=0;r<4;r++)o=rs44_process(&st,in,1024,out);assert(out[o*2-2]>15990&&out[o*2-2]<16010);
 printf("44.1->48 kHz resampler: frames, phase continuity, chunk independence, accuracy PASS\n");return 0;}
