/* Host simulation compiles the EXACT production session source. The CF and
 * AirPlay shims below never replace the QNX binary; they exercise its state
 * machine and actual callback ordering without access to a real iPhone. */
#define ENABLE_LOGGING 0
#include "../native/altscreen_session.c"
#include <assert.h>

typedef struct mock_obj {
    int kind, count;
    char value[512];
    struct { char key[70]; struct mock_obj *val; } item[20];
} mock_obj;
static mock_obj *obj(int kind, const char *s) {
    mock_obj *v = calloc(1,sizeof(*v)); assert(v); v->kind=kind;
    if(s) snprintf(v->value,sizeof(v->value),"%s",s); return v;
}
static cf_ref fake_str(cf_ref allocator, const char *s, uint32_t enc) { (void)allocator;(void)enc;return obj(3,s); }
static cf_ref fake_dict_new(cf_ref allocator, cf_index cap, const void*k, const void*v){(void)allocator;(void)cap;(void)k;(void)v;return obj(1,0);}
static cf_ref fake_arr_new(cf_ref allocator,cf_index cap,const void*c){(void)allocator;(void)cap;(void)c;return obj(2,0);}
static cf_type_id fake_type(cf_ref x){return x?((mock_obj*)x)->kind:0;}
static cf_type_id fake_dict_type(void){return 1;}
static cf_type_id fake_arr_type(void){return 2;}
static void fake_set(cf_ref d,const void *k,const void *v){
    mock_obj *m=(mock_obj*)d;const char *name=((const mock_obj*)k)->value;int i;
    for(i=0;i<m->count;i++)if(!strcmp(m->item[i].key,name))break;
    assert(i<20);
    if(i==m->count)m->count++;
    snprintf(m->item[i].key,sizeof(m->item[i].key),"%s",name);
    m->item[i].val=(mock_obj*)v;
}
static const void *fake_get(cf_ref d,const void*k){
    mock_obj *m=(mock_obj*)d;const char *name=((const mock_obj*)k)->value;int i;
    for(i=0;i<m->count;i++)if(!strcmp(m->item[i].key,name))return m->item[i].val;
    return NULL;
}
static os_status fake_set_i64(cf_ref d,const void*k,int64_t v){
    mock_obj *n=obj(4,NULL);snprintf(n->value,sizeof(n->value),"%lld",(long long)v);
    fake_set(d,k,n);return 0;
}
static cf_ref fake_number(cf_ref allocator,int type,const void*v){
    (void)allocator;(void)type;
    mock_obj*n=obj(4,NULL);snprintf(n->value,sizeof(n->value),"%lld",(long long)*(const int64_t*)v);return n;
}
static void fake_append(cf_ref arr,const void *v){mock_obj*a=arr; assert(a->count<20);a->item[a->count++].val=(mock_obj*)v;}
static void fake_release(cf_ref x){(void)x;/* fixed-length unit-test run */}

static struct {
    void (*cb)(os_status,cf_ref,void*);
    void *ctx;
    char name[64],url[420];
    int view;
} sends[128];
static int nsends, transport_rc;
static os_status mock_send(void*recv,cf_ref req,void(*cb)(os_status,cf_ref,void*),void*ctx){
    mock_obj *name=(mock_obj*)fake_get(req,obj(3,"type"));
    mock_obj *params=(mock_obj*)fake_get(req,obj(3,"params"));
    mock_obj *url=params?(mock_obj*)fake_get(params,obj(3,"url")):NULL;
    mock_obj *view=params?(mock_obj*)fake_get(params,obj(3,"viewAreaIndex")):NULL;
    int idx=nsends++;(void)recv;
    assert(idx<128);assert(name);
    snprintf(sends[idx].name,sizeof(sends[idx].name),"%s",name->value);
    if(url)snprintf(sends[idx].url,sizeof(sends[idx].url),"%s",url->value);
    sends[idx].view=view?atoi(view->value):-1;
    sends[idx].cb=cb;
    sends[idx].ctx=ctx;
    return transport_rc;
}
static void ack(int i,int status){
    assert(i>=0&&i<nsends);
    assert(sends[i].cb!=NULL);
    sends[i].cb(status,NULL,sends[i].ctx);
    sends[i].cb=NULL;
    sends[i].ctx=NULL;
}
static void step(const char *name){
    int old=nsends;
    v8s_control_tick();
    if(nsends!=old+1){fprintf(stderr,"STEP_FAILED expected=%s old=%d new=%d stage=%d pending=%d startup=%d marker=%d\n",name,old,nsends,g_states[0].control_stage,g_states[0].pending_view,g_states[0].control_startup,access(ALTSCREEN_ACTIVE_MARKER,F_OK)==0); abort();}
    assert(strcmp(sends[old].name,name)==0);
    assert(sends[old].cb!=NULL);
}
static void idle(void){int old=nsends;v8s_control_tick();assert(nsends==old);}
static void prepare(void){
    memset(&g_api,0,sizeof(g_api));
    g_api.string_create=fake_str;
    g_api.dict_create_mutable=fake_dict_new;
    g_api.array_create_mutable=fake_arr_new;
    g_api.get_type=fake_type;
    g_api.dict_type=fake_dict_type;
    g_api.array_type=fake_arr_type;
    g_api.dict_get_value=fake_get;
    g_api.dict_set_value=fake_set;
    g_api.dict_set_i64=fake_set_i64;
    g_api.number_create=fake_number;
    g_api.array_append=fake_append;
    g_api.release=fake_release;
    g_api.send_command=mock_send;
    g_api.ready=1;
}
int main(void){
    altscreen_state *s;int b;
    prepare();
    pthread_mutex_lock(&g_lock);
    s=alloc_state_locked((void*)0x1001);
    s->screen_started=1;
    s->route_known=1;
    s->route_active=1;
    pthread_mutex_unlock(&g_lock);
    /* First-frame activation must work even when active marker is absent. */
    request_cluster(s);
    step("showUI"); assert(strstr(sends[0].url,"leftAligned"));idle();
    ack(0,0);step("updateViewArea"); assert(sends[1].view==1);idle();
    ack(1,0);step("forceKeyFrame");ack(2,0);idle();
    assert(s->last_ui_slot==0&&s->current_view==1);
    /* VC becomes active once first frame is presented. */
    FILE *marker=fopen(ALTSCREEN_ACTIVE_MARKER,"w");assert(marker);fclose(marker);
    /* Same-gauge route transition: NO showUI when URL unchanged. */
    s->route_active=0;queue_route_view_for_receiver(s->receiver,0);
    step("updateViewArea");assert(sends[3].view==0);ack(3,0);
    step("forceKeyFrame");ack(4,0);idle();
    /* Big gauges: one topAligned show, ack then matching view then IDR. */
    s->route_active=1;
    g_layout_slot=1;queue_route_view_for_receiver(s->receiver,0);
    step("showUI");assert(strstr(sends[5].url,"topAligned"));idle();
    ack(5,0);step("updateViewArea");assert(sends[6].view==3);
    ack(6,0);step("forceKeyFrame");ack(7,0);idle();
    /* Rapid VIEW changes DURING pending showUI: newest wins. */
    g_layout_slot=0;queue_route_view_for_receiver(s->receiver,0);
    step("showUI");assert(strstr(sends[8].url,"leftAligned"));
    g_layout_slot=1;queue_route_view_for_receiver(s->receiver,0);
    idle();ack(8,0);
    step("showUI");assert(strstr(sends[9].url,"topAligned"));
    ack(9,0);step("updateViewArea");assert(sends[10].view==3);
    ack(10,0);step("forceKeyFrame");ack(11,0);idle();
    /* Command refusal: no dependent step may be sent. */
    s->route_active=0;queue_route_view_for_receiver(s->receiver,0);
    transport_rc=-1; b=nsends;v8s_control_tick();assert(nsends==b+1);
    assert(!strcmp(sends[b].name,"updateViewArea"));idle();
    assert(s->control_stage==ALTSCREEN_CONTROL_IDLE);
    transport_rc=0;
    /* Failed AirPlay completion: abort the sequence. */
    g_layout_slot=0;queue_route_view_for_receiver(s->receiver,0);
    step("showUI");b=nsends-1;ack(b,-42);idle();
    assert(s->last_ui_slot==-1);
    /* Timeout invalidates request token, late reply cannot change state. */
    queue_route_view_for_receiver(s->receiver,0);
    step("showUI");b=nsends-1;
    s->control_deadline_ms=now_ms()-1;
    idle();assert(!s->control_awaiting_ack&&s->control_stage==ALTSCREEN_CONTROL_IDLE);
    ack(b,0);assert(s->last_ui_slot==-1);idle();
    /* Stale callback from previous CarPlay generation cannot affect a reused
     * receiver, even when the previous request was still awaiting ack. */
    queue_route_view_for_receiver(s->receiver,0);
    step("showUI");b=nsends-1;
    /* Fresh CarPlay session has its own generation and reissues showUI. */
    pthread_mutex_lock(&g_lock);
    memset(s,0,sizeof(*s));
    s=alloc_state_locked((void*)0x1001);
    assert(s);s->screen_started=1;
    pthread_mutex_unlock(&g_lock);
    request_cluster(s);
    step("showUI");assert(strstr(sends[nsends-1].url,"leftAligned"));
    assert(s->control_awaiting_ack);
    ack(b,0); /* old sender response arrives after reuse */
    assert(s->control_awaiting_ack && s->last_ui_slot==-1);
    ack(nsends-1,0);step("updateViewArea");ack(nsends-1,0);
    step("forceKeyFrame");ack(nsends-1,0);
    unlink(ALTSCREEN_ACTIVE_MARKER);
    puts("V8S_ASYNC_CONTROL_MOCK=PASS startup/acks/coalescing/same-url/timeout/stale/reconnect/transport-refusal");
    return 0;
}
