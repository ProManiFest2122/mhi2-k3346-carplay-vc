/* Emulator-only: actual K3346 CAudioDriverIF constructor/open/configure/write.
 * No phone negotiation, no vehicle deployment. */
#include <stddef.h>
extern void driver_ctor(void *,const char *) __asm__("_ZN14CAudioDriverIFC1EPKc");
extern int driver_open(void *) __asm__("_ZN14CAudioDriverIF4openEv");
extern int driver_configure(void *,unsigned,unsigned,unsigned) __asm__("_ZN14CAudioDriverIF9configureEjjj");
extern int driver_write(void *,const unsigned char *,unsigned,unsigned) __asm__("_ZN14CAudioDriverIF5writeEPKhjj");
extern void driver_close(void *) __asm__("_ZN14CAudioDriverIF5closeEv");
extern int snprintf(char *,size_t,const char *,...),write(int,const void *,size_t);extern void exit(int);
static unsigned long long object[32];static short pcm[512*2];
static void report(const char *name,int status){char text[128];int n=snprintf(text,sizeof(text),"AIRPLAY_DRIVER_NATIVE %s=%d\n",name,status);write(1,text,n);}
void _start(void){
 driver_ctor(object,"MPL1");int status=driver_open(object);report("open",status);if(!status)exit(1);
 status=driver_configure(object,48000,16,2);report("configure_same_rate",status);if(!status)exit(2);
 unsigned errors=0;
 for(unsigned b=0;b<32;b++){for(unsigned i=0;i<512;i++){unsigned t=b*512+i;pcm[i*2]=(short)(((int)(t%97)-48)*61);pcm[i*2+1]=(short)(((int)(t%137)-68)*47);}errors+=driver_write(object,(const unsigned char *)pcm,sizeof(pcm),250)!=0;}
 report("write_errors",errors);
 driver_close(object);unsigned cleared=((unsigned *)object)[1]==0;report("close_handle_cleared",cleared);exit(errors||!cleared?3:0);
}
