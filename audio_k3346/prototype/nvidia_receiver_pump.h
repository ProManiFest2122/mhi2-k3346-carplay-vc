#ifndef MHI2_NVIDIA_RECEIVER_PUMP_H
#define MHI2_NVIDIA_RECEIVER_PUMP_H
#include "nvidia_pcm_sink.h"
struct mhi2_buffered_source {
 size_t (*peek)(int16_t *,size_t,unsigned);
 size_t (*commit)(size_t,unsigned);
 size_t (*pending)(unsigned);
};
/* A single output consumer; control operations must not hold the source lock.
 * The receiver keeps ownership of decoding, clock, route and priming policy.
 * This component owns only a bounded peek -> native write -> commit transaction. */
struct mhi2_nv_receiver_pump {
 pthread_mutex_t mutex;
 struct mhi2_nv_sink *sink;
 struct mhi2_buffered_source source;
 unsigned generation;
 int initialized,fault;
 int16_t scratch[1024*6];
};
int mhi2_nv_receiver_pump_init(struct mhi2_nv_receiver_pump *,struct mhi2_nv_sink *,const struct mhi2_buffered_source *);
struct mhi2_nv_result mhi2_nv_receiver_pump_once(struct mhi2_nv_receiver_pump *);
/* Refuses to replay an ambiguous AU. Receiver must first discard/reset its
 * queued timeline AND explicitly reopen the native connection. No automatic
 * recovery or assertion that a native reopen purges downstream playback. */
int mhi2_nv_receiver_pump_rearm(struct mhi2_nv_receiver_pump *);
/* Caller joins every consumer/control thread before destroy. Sink remains owned by caller. */
int mhi2_nv_receiver_pump_destroy(struct mhi2_nv_receiver_pump *);
#endif
