#include "nvidia_receiver_pump.h"
#include <string.h>
int mhi2_nv_receiver_pump_init(struct mhi2_nv_receiver_pump *p,struct mhi2_nv_sink *s,const struct mhi2_buffered_source *source){
 if(!p||!s||!source||!source->peek||!source->commit||!source->pending)return MHI2_NV_INVALID;
 unsigned generation=mhi2_nv_sink_generation(s);
 if(!generation)return MHI2_NV_INVALID;
 memset(p,0,sizeof(*p));if(pthread_mutex_init(&p->mutex,0))return MHI2_NV_INVALID;
 p->sink=s;p->source=*source;p->generation=generation;p->initialized=1;return 0;
}
struct mhi2_nv_result mhi2_nv_receiver_pump_once(struct mhi2_nv_receiver_pump *p){
 struct mhi2_nv_result r={0,0,MHI2_NV_INVALID};
 if(!p||!p->initialized)return r;
 pthread_mutex_lock(&p->mutex);
 if(p->fault){r.status=p->fault;goto done;}
 size_t offered=p->source.peek(p->scratch,1024,6);
 if(!offered){r.status=0;goto done;}
 if(offered>1024){p->source.commit(0,6);p->fault=r.status=MHI2_NV_INVALID;goto done;}
 r=mhi2_nv_sink_write(p->sink,p->generation,p->scratch,offered);
 /* Always release the reservation, even on stale generation or an error.
  * Only a known accepted prefix can leave the receiver ring. */
 if(p->source.commit(r.confirmed_frames,6)!=r.confirmed_frames){
  p->fault=r.status=MHI2_NV_INVALID;
 }else if(r.status){p->fault=r.status;}
done:
 pthread_mutex_unlock(&p->mutex);return r;
}
int mhi2_nv_receiver_pump_rearm(struct mhi2_nv_receiver_pump *p){
 int r=MHI2_NV_INVALID;if(!p||!p->initialized)return r;
 pthread_mutex_lock(&p->mutex);
 unsigned generation=mhi2_nv_sink_generation(p->sink);
 if(generation&&generation!=p->generation&&!p->source.pending(6)){
  struct mhi2_nv_result ready=mhi2_nv_sink_write(p->sink,generation,0,0);
  r=ready.status;if(!r){p->generation=generation;p->fault=0;}
 }
 pthread_mutex_unlock(&p->mutex);return r;
}
int mhi2_nv_receiver_pump_destroy(struct mhi2_nv_receiver_pump *p){
 if(!p||!p->initialized)return MHI2_NV_INVALID;
 int r=pthread_mutex_destroy(&p->mutex);if(!r)p->initialized=0;return r;
}
