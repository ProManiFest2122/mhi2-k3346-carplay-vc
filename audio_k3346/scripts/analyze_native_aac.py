#!/usr/bin/env python3
"""Validate native direct/backend table canary records without hiding host drift."""
import argparse
import json
import re
from pathlib import Path


def main():
    ap=argparse.ArgumentParser();ap.add_argument('run',type=Path);args=ap.parse_args()
    text=(args.run/'mhi2-debug-console.log').read_text(errors='replace')
    direct=re.findall(r'AAC_NATIVE pass=(\d+) detail=(\d+)',text)
    bridge=re.findall(r'AAC_NATIVE bridge_pass=(\d+) detail=(\d+)',text)
    peaks=re.findall(r'AAC_NATIVE left_peak=(\d+) detail=(\d+)',text)
    finish=re.findall(r'AAC_NATIVE PASS=(\d+) detail=(\d+)',text)
    host=json.loads((Path(__file__).resolve().parents[1]/'artifacts/fdk-decoder-host/analysis.json').read_text())
    expected_indices=['0','1']
    hashes=[int(v) for _,v in direct+bridge]
    passed=([i for i,_ in direct]==expected_indices and [i for i,_ in bridge]==expected_indices
            and len(set(hashes))==1 and len(peaks)==2
            and all([int(a),int(b)]==host['channel_peak'] for a,b in peaks)
            and finish==[(str(host['packets']),str(host['frames']))]
            and 'AAC_NATIVE limiter_disabled=1 detail=0' in text
            and re.search(r'AAC_NATIVE_EXIT=0\b',text) is not None)
    result=dict(passed=bool(passed),direct_runs=direct,codec_table_runs=bridge,channel_peaks=peaks,
                native_pcm_fnv1a32=hashes[0] if hashes else None,host_pcm_fnv1a32=host['pcm_fnv1a32'],
                host_native_bit_exact=bool(hashes and hashes[0]==host['pcm_fnv1a32']),
                implicit_peak_limiter=False,
                scope='QNX dynamic raw AAC-LC and reference codec-table backend; recreate/reset determinism; not full native/host PCM equivalence or receiver playback')
    (args.run/'aac-analysis.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));return int(not passed)


if __name__=='__main__':raise SystemExit(main())
