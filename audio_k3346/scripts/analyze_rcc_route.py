#!/usr/bin/env python3
"""Exact K3346 CarPlay route profile. Evidence only; no flashable output."""
import gzip,hashlib,json,struct
from pathlib import Path
A=Path(__file__).resolve().parents[1]
EXPECTED='e12870cd748e59ea28e1d0cb2499312238d3f30539684d389ca031cebd5d4993'
def profile(data):
 if hashlib.sha256(data).hexdigest()!=EXPECTED:raise ValueError('Unsupported MIBRoot fingerprint')
 needle=struct.pack('<5I',0x78,0x46e,1,0x46f,2)
 if data.count(needle)!=1:raise ValueError('Ambiguous DIO route resource')
 offset=data.index(needle)
 return dict(resource=0x78,record_file_offset=offset,internal=dict(source=0x46e,sink=1),external=dict(source=0x46f,sink=2),required_internal_candidate=dict(source=0x46f,sink=2),changes=[dict(offset=offset+4,old='6e040000',new='6f040000'),dict(offset=offset+8,old='01000000',new='02000000')])
def main():
 root=A/'artifacts/rcc-k3346-stage2-extracted';p=(root/'usr/apps/MIBRoot').read_bytes();result=profile(p)
 table=(root/'etc/MIB_AudioTable_AU_G22.xml').read_bytes();live=(A/'handoff-windows/MHI2_RETURN_20261015-20261015-073857/evidence/live-comms/rcc-audiotable-G22.bin').read_bytes()
 assert table==live
 xml=gzip.decompress(table)
 assert b'LS_ENT_AMP_DIO_MEDIA.MC' in xml and b'LC_ENT_AMP_DIO_MEDIA.MULTICHANNEL' in xml
 result.update(source_sha256=EXPECTED,audiotable_live_matches_stock=True,audiotable_sha256=hashlib.sha256(table).hexdigest(),status='STATIC_PROFILE_ONLY_NO_INSTALLER',remaining=['Confirm installed RCC MIBRoot/DSP fingerprints on target','Resolve media-specific ARC reader geometry without changing voice readers','Resolve PreDownMixer/MainMix channel mapping and gains for SignalFlow31','Verify INIC bandwidth and physical six-channel output','Package whole RCC chain with tested rollback before enabling type103'])
 (A/'artifacts/rcc-route-analysis.json').write_text(json.dumps(result,indent=2)+'\n')
 assert profile(p)=={k:result[k] for k in ['resource','record_file_offset','internal','external','required_internal_candidate','changes']}
 try:profile(p[:-1]+bytes([p[-1]^1]))
 except ValueError:pass
 else:raise AssertionError('wrong binary accepted')
 print('Exact RCC route profile PASS; live audio table identical; no deployment generated')
if __name__=='__main__':main()
