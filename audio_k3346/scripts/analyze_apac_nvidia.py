#!/usr/bin/env python3
"""Compare selected principal-S16 TDM windows to committed Apple APAC PCM.
No inference about RCC, speaker wiring or full-stream gap-free playback.
"""
import argparse,json,struct
from pathlib import Path
import numpy as np
from scipy.signal import correlate
from analyze_native_tdm import decode
A=Path(__file__).resolve().parents[1]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('run',type=Path);args=ap.parse_args()
 p=(A/'reference-audio-a63d547/apac_decoder/tests/fixtures/frames.bin').read_bytes()
 packets,channels,frames,cookiebytes=struct.unpack_from('<4I',p,8);assert (packets,channels,frames)==(6,6,1024)
 at=24+cookiebytes;lengths=struct.unpack_from('<6I',p,at);at+=24+sum(lengths)
 ref=np.frombuffer(p,dtype='<f4',offset=at).reshape(-1,6)[:,[1,2,0,5,3,4]]
 scaled=ref.astype(np.float64)*32768
 x=np.clip(np.trunc(scaled+np.where(scaled>=0,0.5,-0.5)),-32768,32767).astype(np.int16)
 raw=(args.run/'native-tdm0.raw').read_bytes();index=[tuple(map(int,line.split())) for line in (args.run/'native-tdm0.index').read_text().splitlines()]
 segments=[decode(raw[o:o+n]) for _,o,n,marker in index if marker==96];assert segments,'No APAC test marker'
 y=np.concatenate(segments);width=256;rows=[]
 for pos in range(1024,len(x)-width+1,512):
  q=x[pos:pos+width].astype(np.float64)
  if np.count_nonzero(q)<width:continue
  if len(y)<width:break
  scores=np.zeros(len(y)-width+1)
  for c in range(6):
   v=y[:,c].astype(np.float64);sq=np.r_[0,np.cumsum(v*v)]
   scores+=sq[width:]-sq[:-width]+np.sum(q[:,c]**2)-2*correlate(v,q[:,c],mode='valid',method='fft')
  best=int(np.argmin(scores));delta=y[best:best+width].astype(np.int32)-x[pos:pos+width].astype(np.int32)
  rows.append(dict(reference_frame=pos,capture_frame=best,frames=width,max_error_lsb=int(np.abs(delta).max()),rms_error_lsb=float(np.sqrt(np.mean(delta.astype(float)**2)))))
 matched=sum(r['max_error_lsb']<=1 for r in rows)
 peaks=np.abs(y.astype(np.int32)).max(axis=0).tolist() if len(y) else [0]*6
 ref_peaks=np.abs(x.astype(np.int32)).max(axis=0).tolist()
 active=[i for i,v in enumerate(ref_peaks) if v>0]
 result=dict(scope='Dynamic APAC codec -> declared PCM6 -> native NVIDIA -> emulated TDM principal S16; selected windows only; no RCC/speakers',marked_frames=len(y),channel_peak=peaks,reference_peak=ref_peaks,reference_active_channels=active,all_six_independence_requires_separate_onehot=True,windows=rows,windows_within_1lsb=matched,passed=matched>=4 and all(peaks[i]>0 for i in active))
 (args.run/'apac-nvidia-tdm-analysis.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));return int(not result['passed'])
if __name__=='__main__':raise SystemExit(main())
