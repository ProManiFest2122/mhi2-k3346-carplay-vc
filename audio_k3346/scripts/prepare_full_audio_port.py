#!/usr/bin/env python3
"""Stage exact-profile full reference receiver sources; not an installable NV port."""
import hashlib,json,re
from pathlib import Path
A=Path(__file__).resolve().parents[1]
EXPECTED='34503a9f18799420005d7bd40c3cbf84634c8c9030bb0dcba79d519f0227e949'
def inspect(data):
 if hashlib.sha256(data).hexdigest()!=EXPECTED:raise ValueError('Unknown libairplay image')
 # Static anchors recovered from K3346 disassembly, not MU1316 offsets.
 checks={0x2a164:'f0452de9dc419fe5dc619fe5',0x2a7f8:'f0412de9c85090e50370a0e1',0x2b9f0:'f04f2de91c3002e3'}
 # The first three verified byte sequences are the runtime dispatcher boundaries.
 for va,hexdata in checks.items():
  if data[va:va+len(bytes.fromhex(hexdata))]!=bytes.fromhex(hexdata):raise ValueError('Code anchor mismatch at '+hex(va))
 return dict(name='MHI2 K3346 AirPlay 210.81 NVIDIA',image_sha256=EXPECTED,ensureCreatedVA=0x2a164,setPlistResponseVA=0x2a7f8,httpProcessVA=0x2b9f0,serverGlobalVA=0xab02c,httpProcessSlotOffset=0x64,sessionClockOffset=0x13c8,sessionFamilyOffset=0x51,audioStreamTypeOffset=108,audioStreamAudioTypeOffset=112,httpMethodPointerOffset=0x201c,httpMethodLengthOffset=0x2020,httpBodyPointerOffset=0x209c,httpBodyLengthOffset=0x20a8,ensureCreatedPrologue=data[0x2a164:0x2a164+12].hex(),setPlistResponsePrologue=data[0x2a7f8:0x2a7f8+12].hex())
def nvidia_variant(source):
 """NVIDIA-sink variant: QSA calls resolve to prototype/nvidia_qsa_shim.h; anchors must match exactly."""
 anchor='#define SPA_PCM_SINK_NAME "mpl5_dio_ent"'
 if source.count(anchor)!=1:raise ValueError('sink anchor')
 source=source.replace(anchor,'#include "nvidia_qsa_shim.h"\n'+anchor,1)
 source,n=re.subn(r'dlsym\(RTLD_NEXT,\s*"(snd_pcm_[a-z_]+)"\)',r'spa_nv_shim_resolve("\1")',source)
 if n<19 or re.search(r'RTLD_NEXT,\s*"snd_pcm',source):raise ValueError('QSA lookups not fully replaced (%d)'%n)
 source,n=re.subn(r'HOOK_EXPORT (int|ssize_t) (snd_pcm_[a-z_]+)\(',r'static __attribute__((unused)) \1 \2(',source)
 if n!=8:raise ValueError('expected 8 snd_pcm exports, found %d'%n)
 old='if ((written == -EIO || written == -ETIMEDOUT) &&\n            spa_pcm_recover_underrun(handle, 1)) {'
 if source.count(old)!=1:raise ValueError('detached recovery anchor')
 # No inline retry: a native failure is ambiguous, so the same period must never be replayed.
 source=source.replace(old,'if (0 && spa_pcm_recover_underrun(handle, 1)) {',1)
 old2='(void)spa_buffered_pcm_commit(acceptedFrames, channels);\n        writeNs = spa_monotonic_ns();'
 if source.count(old2)!=1:raise ValueError('detached commit anchor')
 # One commit: a second call after a partial commit would find the reservation already released.
 new2='{ size_t dropFrames = spa_nv_shim_take_uncertain_frames();\n          if (acceptedFrames + dropFrames > got) dropFrames = got - acceptedFrames;\n          (void)spa_buffered_pcm_commit(acceptedFrames + dropFrames, channels);\n          if (dropFrames) {\n            pthread_mutex_lock(&spa_pcm_sink.lock);\n            spa_pcm_sink.detachedNeedsPrepare = 1;\n            pthread_mutex_unlock(&spa_pcm_sink.lock);\n          } }\n        writeNs = spa_monotonic_ns();'
 source=source.replace(old2,new2,1)
 # Live MMI level: our Java writes /tmp/mhi2_audio_level and there is no framework bus (CMD_UPMIX),
 # so the detached writer polls the file (rate limited) instead of reading it only at stream start.
 a='volatile int spa_upmix_level = SPA_UPMIX_LEVEL_OFF;'
 if source.count(a)!=1:raise ValueError('level anchor')
 poll='''#undef SPA_UPMIX_LEVEL_FILE
#define SPA_UPMIX_LEVEL_FILE "/tmp/mhi2_audio_level"
#ifndef SPA_UPMIX_LEVEL_POLL_NS
#define SPA_UPMIX_LEVEL_POLL_NS UINT64_C(200000000)
#endif
'''
 source=source.replace(a,poll+a,1)
 b='int spa_upmix_level_reload(void)\n{'
 if source.count(b)!=1:raise ValueError('level reload anchor')
 source=source.replace(b,'int spa_upmix_level_reload(void);\nstatic int spa_upmix_level_poll(void)\n{\n    static uint64_t nextNs;\n    struct timespec ts;\n    uint64_t now;\n    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;\n    now = (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;\n    if (now < nextNs) return 0;\n    nextNs = now + SPA_UPMIX_LEVEL_POLL_NS;\n    return spa_upmix_level_reload();\n}\n\n'+b,1)
 c='        spa_pcm_io_lock();\n        pthread_mutex_lock(&spa_pcm_sink.lock);\n        if (!spa_pcm_sink.detached || !spa_pcm_sink.handle ||'
 if source.count(c)!=1:raise ValueError('detached entry anchor')
 source=source.replace(c,'        (void)spa_upmix_level_poll();\n'+c,1)
 # Fail closed: without the RCC-qualified build flag the hook must neither advertise 5.1 nor enable spatial/realtime PCM,
 # whatever the process environment says (an advertised 6ch stream with a refused 6ch sink would break CarPlay audio).
 a='#include "audio_hook.h"\n'
 if source.count(a)!=1:raise ValueError('header anchor')
 source=source.replace(a,'#ifndef MHI2_NV_SIX_CHANNEL_QUALIFIED\n#define MHI2_NV_SIX_CHANNEL_QUALIFIED 0\n#endif\n'+a,1)
 for old,new in (('spa_audio_config_value.atmos = atmos;','spa_audio_config_value.atmos = MHI2_NV_SIX_CHANNEL_QUALIFIED ? atmos : 0;'),
                 ('spa_audio_config_value.buffered = spatial || atmos;','spa_audio_config_value.buffered = MHI2_NV_SIX_CHANNEL_QUALIFIED ? (spatial || atmos) : 0;'),
                 ('spa_audio_config_value.realtimePCM = spatial || atmos;','spa_audio_config_value.realtimePCM = MHI2_NV_SIX_CHANNEL_QUALIFIED ? (spatial || atmos) : 0;')):
  if source.count(old)!=1:raise ValueError('config anchor '+old)
  source=source.replace(old,new,1)
 source+='\n#include "nvaudio_stock_bridge.h"\n'
 return source
def main():
 data=(A/'artifacts/stock/eso/lib/libairplay.so').read_bytes();p=inspect(data)
 ref=A/'reference-audio-a63d547/hook/audio/audio_hook.c';source=ref.read_text()
 start=source.index('static const spa103_private_profile_t spa103_private_profiles[] = {');end=source.index('\n};',start)+3
 def bytes_c(hexdata):return ', '.join('0x'+hexdata[i:i+2] for i in range(0,len(hexdata),2))
 record='''static const spa103_private_profile_t spa103_private_profiles[] = {
    {
        "MHI2 K3346 AirPlay 210.81 NVIDIA",
        0x0002a164u, 0x0002a7f8u, 0x0002b9f0u, 0x000ab02cu,
        0x64u, 5064u, 81u, 108u, 112u,
        0x201cu, 0x2020u, 0x209cu, 0x20a8u,
        { ENSURE },
        { PLIST }
    }
};'''.replace('ENSURE',bytes_c(p['ensureCreatedPrologue'])).replace('PLIST',bytes_c(p['setPlistResponsePrologue']))
 source=source[:start]+record+source[end:]
 source=source.replace('volatile int spa_upmix_level = SPA_UPMIX_LEVEL_NORMAL;','volatile int spa_upmix_level = SPA_UPMIX_LEVEL_OFF;')
 out=A/'artifacts/full-audio/port';out.mkdir(parents=True,exist_ok=True)
 (out/'audio_hook_k3346.c').write_text(source)
 (out/'audio_hook_k3346_nv.c').write_text(nvidia_variant(source))
 p.update(status='STATIC_PROFILE_STAGED_NOT_DEPLOYABLE',source_reference='74b0d70d7cae89061cd586f854bf2cc68c8fd50f',notes=['Native NVIDIA sink adapter is not implemented by the reference QSA code','RCC internal routing remains gated; do not advertise PCM6 on the stock stereo route','Static family/clock derivation still needs live SETUP validation'],evidence=['logs/k3346-ensure-created.txt','logs/k3346-http-process.txt','logs/k3346-setup-http.txt','logs/k3346-clock-slot.txt'])
 (out/'k3346-profile.json').write_text(json.dumps(p,indent=2)+'\n')
 try:inspect(data[:-1]+bytes([data[-1]^1]))
 except ValueError:pass
 else:raise AssertionError('Unknown image accepted')
 print('K3346 private profile anchors PASS; receiver source staged, not deployed')
if __name__=='__main__':main()
