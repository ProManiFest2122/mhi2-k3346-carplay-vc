/* Emulator-only: spawn() a #!/bin/sh shim by absolute path with mm-renderer-like arguments (what the stock
 * osal ProcessTNG/medialauncher does) and report the child's output pid/exit. Usage: native-spawn-probe <path> */
#include <stddef.h>
extern int spawn(const char *,int,int *,const void *,char *const[],char *const[]);
extern int waitpid(int,int *,int);extern int getpid(void);extern long write(int,const void *,unsigned long);extern void exit(int);
extern char **environ;
static void put(const char *s){unsigned long n=0;while(s[n])n++;write(1,s,n);}
static void num(int v){char b[16];int i=15;b[i]=0;if(v<0)v=-v;do{b[--i]='0'+v%10;v/=10;}while(v);put(b+i);}
int main(int argc,char **argv){
 char *av[]={(char *)"mm-renderer",(char *)"-r",(char *)"/ramdisk/pps/multimedia",(char *)"-k",NULL};
 int st=0,pid;
 if(argc<2)return 2;
 pid=spawn(argv[1],0,NULL,NULL,av,environ);
 put("SPAWNPROBE_PID_OK=");num(pid>0);put("\n");
 if(pid<0){put("SPAWNPROBE_FAIL\n");return 3;}
 waitpid(pid,&st,0);
 put("SPAWNPROBE_EXIT=");num((st>>8)&255);put("\n");return 0;
}
