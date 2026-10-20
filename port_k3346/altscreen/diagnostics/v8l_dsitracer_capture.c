/* v8l read-only DSITracer capture. Logs explicit startup/lock/connection
 * state for both observed ports. Reads only from 127.0.0.1; never sends data
 * to DSI or modifies RGI, VC or renderer state. Rebuild from source only. */
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

#define TRACE_PORT_DEFAULT 1798
#define TRACE_MAX_BYTES (512*1024)
#define TRACE_FILE_COUNT 4
#define TRACE_LINE_MAX 8192
static char pid_path[90];
static char status_path[90];
static int trace_port=TRACE_PORT_DEFAULT;
static int raw_mode=0;
static size_t total_received=0;
static time_t last_heartbeat=0;
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
    FILE *f=fopen(status_path,"a");
    fprintf(stderr,"[v8l-capture port=%d] %s\n",trace_port,msg);
    fflush(stderr);
    if (!f) return;
    /* status file capped; it is not necessary to capture repeated retry spam. */
    if (ftell(f)>131072L) { fclose(f); return; }
    fprintf(f,"%ld %s\n",(long)time(NULL),msg);
    fclose(f);
}
static int lock_single_instance(void) {
    char pidbuf[32];
    long prev=0;
    int attempt;
    for(attempt=0;attempt<2;attempt++) {
        lock_fd=open(pid_path,O_WRONLY|O_CREAT|O_EXCL,0644);
        if(lock_fd>=0) break;
        if(errno!=EEXIST) { status("PIDFILE_CREATE_FAILED"); return 0; }
        {
            FILE *f=fopen(pid_path,"r");
            if(f){if(fscanf(f,"%ld",&prev)!=1)prev=0;fclose(f);}
        }
        if(prev>0 && kill((pid_t)prev,0)==0) {status("ALREADY_RUNNING");return 0;}
        if(unlink(pid_path)!=0){status("STALE_PIDFILE_REMOVE_FAILED");return 0;}
        status("STALE_PIDFILE_RECREATED");
    }
    if(lock_fd<0) {status("PID_LOCK_FAILED");return 0;}
    snprintf(pidbuf,sizeof(pidbuf),"%ld\n",(long)getpid());
    if(write(lock_fd,pidbuf,strlen(pidbuf))<0){status("PIDFILE_WRITE_FAILED");close(lock_fd);lock_fd=-1;unlink(pid_path);return 0;}
    return 1;
}
static int open_next_file(void) {
    char path[96];
    if(trace_file){ fflush(trace_file);fclose(trace_file);trace_file=NULL; }
    snprintf(path,sizeof(path),raw_mode ? "/tmp/v8l-dsitracer-20991-%02d.raw" : "/tmp/v8l-dsitracer-1798-%02d.log",trace_slot);
    trace_slot=(trace_slot+1)%TRACE_FILE_COUNT;
    trace_file=fopen(path,raw_mode ? "wb" : "w");
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
    a.sin_port=htons(trace_port);
    a.sin_addr.s_addr=inet_addr("127.0.0.1");
    if(connect(s,(struct sockaddr*)&a,sizeof(a))!=0){close(s);return -1;}
    return s;
}
static void consume_raw(const unsigned char *src,int n) {
    if(n<=0) return;
    if(!trace_file || trace_bytes+(size_t)n>=TRACE_MAX_BYTES){
        if(!open_next_file())return;
    }
    if(fwrite(src,1,(size_t)n,trace_file)!=(size_t)n)status("RAW_WRITE_FAILED");
    else { trace_bytes+=(size_t)n; fflush(trace_file); }
}
int main(int argc,char **argv) {
    int s,n;
    struct timeval timeout;
    unsigned char buf[4096];
    char info[120];
    time_t now;
    if(argc>2 || (argc==2 && strcmp(argv[1],"1798") && strcmp(argv[1],"20991"))) {
        fprintf(stderr,"usage: v8l-dsitracer-capture [1798|20991]\n");return 3;
    }
    if(argc==2)trace_port=atoi(argv[1]);
    raw_mode=(trace_port==20991);
    snprintf(pid_path,sizeof(pid_path),"/tmp/v8l-dsitracer-%d.pid",trace_port);
    snprintf(status_path,sizeof(status_path),"/tmp/v8l-dsitracer-%d-status.log",trace_port);
    signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGHUP,stop);
    snprintf(info,sizeof(info),"PROCESS_ENTRY pid=%ld port=%d raw=%d",(long)getpid(),trace_port,raw_mode);
    status(info); /* Before lock: even an early exit must be diagnosable. */
    if(!lock_single_instance())return 1;
    status("LOCK_ACQUIRED");
    if(!open_next_file()){unlink(pid_path);return 2;}
    status("RING_OPENED");
    last_heartbeat=time(NULL);
    while(running) {
        s=connect_trace();
        if(s<0){status("CONNECT_FAILED_RETRY");sleep(2);continue;}
        status("CONNECTED");
        timeout.tv_sec=3;timeout.tv_usec=0;
        setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
        while(running) {
            n=recv(s,buf,sizeof(buf),0);
            if(n>0){
                total_received+=(size_t)n;
                if(raw_mode)consume_raw(buf,n);else consume(buf,n);
            } else if(n<0 && (errno==EINTR||errno==EAGAIN||errno==EWOULDBLOCK)) { /* polling only */ }
            else break;
            now=time(NULL);
            if(now-last_heartbeat>=30){
                snprintf(info,sizeof(info),"HEARTBEAT bytes=%lu slot=%d",(unsigned long)total_received,trace_slot);
                status(info);last_heartbeat=now;
            }
        }
        close(s);if(!raw_mode)write_line();
        if(running){status("DISCONNECTED_RETRY");sleep(2);}
    }
    if(!raw_mode)write_line();
    if(trace_file)fclose(trace_file);
    snprintf(info,sizeof(info),"STOP_CAPTURE bytes=%lu",(unsigned long)total_received);
    status(info);
    if(lock_fd>=0)close(lock_fd);
    unlink(pid_path);
    return 0;
}
