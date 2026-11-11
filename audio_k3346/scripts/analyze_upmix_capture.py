#!/usr/bin/env python3
"""Compare indexed native upmix TDM segments with a host PCM reference.
Underflow/fade and unmarked native traffic are excluded from match claims.
This is a native transport/DSP check, not RCC acoustic validation.
"""
import argparse,json
from pathlib import Path
import numpy as np
from scipy.signal import correlate
from analyze_native_tdm import decode

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('run',type=Path);p.add_argument('reference',type=Path);a=p.parse_args()
 raw=(a.run/'native-tdm0.raw').read_bytes()
 rec=[tuple(map(int,l.split())) for l in (a.run/'native-tdm0.index').read_text().splitlines()]
 marked=[r for r in rec if r[3]==64]
 y=np.concatenate([decode(raw[o:o+n]) for _,o,n,_ in marked]) if marked else np.empty((0,6),np.int16)
 x=np.frombuffer(a.reference.read_bytes(),dtype='<i2').reshape(-1,6)
 rows=[];width=512
 # Search each segment against the reference timeline; the emulator's DMA
 # advances during client sleeps, so do not assume contiguous wall-clock audio.
 for pos in range(32768,min(len(y)-width,262144),4096):
  q=y[pos:pos+width].astype(np.float64)
  if np.count_nonzero(q[:,[0,1,4,5]])<width:continue
  scores=np.zeros(len(x)-width+1)
  for c in [0,1,4,5]:
   v=x[:,c].astype(np.float64);sq=np.r_[0,np.cumsum(v*v)]
   scores+=sq[width:]-sq[:-width]+np.sum(q[:,c]**2)-2*correlate(v,q[:,c],mode='valid',method='fft')
  best=int(np.argmin(scores));diff=y[pos:pos+width].astype(np.int32)-x[best:best+width].astype(np.int32)
  rows.append({'capture_frame':pos,'reference_frame':best,'frames':width,'max_error_lsb':int(np.abs(diff).max()),'rms_error_lsb':float(np.sqrt(np.mean(diff.astype(float)**2)))})
 match=sum(r['max_error_lsb']<=1 for r in rows)
 result={'scope':'emulated stereo API fixture -> ARM upmix -> stock TDM; no AirPlay negotiation/RCC', 'marked_frames':len(y),'channel_abs_peak':np.abs(y.astype(np.int32)).max(0).tolist() if len(y) else [],'segments':rows,'segments_within_1lsb':match,'pass':bool(match>=4 and len(y)>2048 and not np.any(y[2048:,[2,3]]))}
 (a.run/'upmix-analysis.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));return int(not result['pass'])
if __name__=='__main__':raise SystemExit(main())
