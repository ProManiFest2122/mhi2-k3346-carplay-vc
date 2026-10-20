/* v8k read-only diagnostic capture of K2161 DSITracer telnet appender.
 * Runs only with AltScreen active, reads localhost:1798, writes bounded /tmp
 * files. Never sends a command to MMI/DSI and never touches renderer/RGI.
 * All changes are in source: binary must be cross-compiled from this C.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define TRACE_PORT 1798
#define TRACE_MAX_BYTES (512*1024)
#define TRACE_FILE_COUNT 4
#define TRACE_LINE_MAX 8192
#define PID_FILE "/tmp/v8k-dsitracer-capture.pid"
#define STATUS_FILE "/tmp/v8k-dsitracer-status.log"
static volatile sig_atomic_t running=1;
static int lock_fd=-1;
static FILE *trace_file=NULL;
static int trace_slot=0;
static size_t trace_bytes=0;
static char linebuf[TRACE_LINE_MAX];
static unsigned int linepos=0;
static int iac_state=0;

static void stop(int sig) { (void)sig; running=0; }
static void status(const char *msg) {
    FILE *f=fopen(STATUS_FILE,"a");
    if (!f) return;
    /* status file capped; it is not necessary to capture repeated retry spam. */
    if (ftell(f)>131072L) { fclose(f); return; }
    fprintf(f,"%ld %s\n",(long)time(NULL),msg);
    fclose(f);
}
static int lock_single_instance(void) {
    struct flock fl;
    char pidbuf[32];
    lock_fd=open(PID_FILE,O_RDWR|O_CREAT,0644);
    if (lock_fd<0) return 0;
    memset(&fl,0,sizeof(fl));
    fl.l_type=F_WRLCK; fl.l_whence=SEEK_SET;
    if (fcntl(lock_fd,F_SETLK,&fl)<0) return 0;
    ftruncate(lock_fd,0);
    snprintf(pidbuf,sizeof(pidbuf),"%ld\n",(long)getpid());
    write(lock_fd,pidbuf,strlen(pidbuf));
    return 1;
}
static int open_next_file(void) {
    char path[96];
    if(trace_file){ fflush(trace_file);fclose(trace_file);trace_file=NULL; }
    snprintf(path,sizeof(path),"/tmp/v8k-dsitracer-%02d.log",trace_slot);
    trace_slot=(trace_slot+1)%TRACE_FILE_COUNT;
    trace_file=fopen(path,"w");
    trace_bytes=0;
    if(!trace_file){status("OPEN_LOG_FAILED");return 0;}
    return 1;
}
static void write_line(void) {
    struct timeval t;
    char prefix[64];
    int n;
    size_t need;
    if(linepos==0) return;
    gettimeofday(&t,NULL);
    n=snprintf(prefix,sizeof(prefix),"%ld.%03ld ",(long)t.tv_sec,(long)(t.tv_usec/1000));
    if(n<0){linepos=0;return;}
    need=(size_t)n+linepos+1;
    if(!trace_file || trace_bytes+need>=TRACE_MAX_BYTES) {
        if(!open_next_file()){linepos=0;return;}
    }
    fwrite(prefix,1,(size_t)n,trace_file);
    fwrite(linebuf,1,linepos,trace_file);
    fputc('\n',trace_file);
    fflush(trace_file);
    trace_bytes+=need;
    linepos=0;
}
static void consume(const unsigned char *src, int count) {
    int i; unsigned char ch;
    for(i=0;i<count;i++) {
        ch=src[i];
        /* Drop RFC 854 telnet negotiation bytes. No responses are sent. */
        if(iac_state==1){
            if(ch==255){iac_state=0;continue;}
            if(ch>=251 && ch<=254){iac_state=2;continue;}
            if(ch==250){iac_state=3;continue;}
            iac_state=0;continue;
        }
        if(iac_state==2){iac_state=0;continue;}
        if(iac_state==3){if(ch==255)iac_state=4;continue;}
        if(iac_state==4){iac_state=(ch==240)?0:3;continue;}
        if(ch==255){iac_state=1;continue;}
        if(ch=='\r')continue;
        if(ch=='\n'){write_line();continue;}
        if(linepos>=TRACE_LINE_MAX-1)write_line();
        linebuf[linepos++]=(char)ch;
    }
}
static int connect_trace(void) {
    int s;
    struct sockaddr_in a;
    s=socket(AF_INET,SOCK_STREAM,0);
    if(s<0)return -1;
    memset(&a,0,sizeof(a));
    a.sin_family=AF_INET;
    a.sin_port=htons(TRACE_PORT);
    a.sin_addr.s_addr=inet_addr("127.0.0.1");
    if(connect(s,(struct sockaddr*)&a,sizeof(a))!=0){close(s);return -1;}
    return s;
}
int main(void) {
    int s,n;
    struct timeval timeout;
    unsigned char buf[4096];
    signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGHUP,stop);
    if(!lock_single_instance()) return 0; /* controller may launch twice */
    status("START_CAPTURE_127.0.0.1_1798");
    if(!open_next_file())return 2;
    while(running) {
        s=connect_trace();
        if(s<0){status("CONNECT_FAILED_RETRY");sleep(2);continue;}
        status("CONNECTED");
        timeout.tv_sec=3;timeout.tv_usec=0;
        setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
        while(running) {
            n=recv(s,buf,sizeof(buf),0);
            if(n>0){consume(buf,n);continue;}
            if(n<0 && (errno==EINTR||errno==EAGAIN||errno==EWOULDBLOCK))continue;
            break;
        }
        close(s);write_line();
        if(running){status("DISCONNECTED_RETRY");sleep(2);}
    }
    write_line();
    if(trace_file)fclose(trace_file);
    status("STOP_CAPTURE");
    /* Lock file remains but fcntl lock is released by exit; can relaunch. */
    if(lock_fd>=0)close(lock_fd);
    return 0;
}
