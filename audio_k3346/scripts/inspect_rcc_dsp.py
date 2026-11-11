#!/usr/bin/env python3
"""Decode stock K3346 BGZ records to read-only C674x evidence, never flash."""
import ctypes,hashlib,json,struct
from pathlib import Path
A=Path(__file__).resolve().parents[1];ROOT=A.parents[1]
p=(ROOT/'runtime/k3346-build/firmware/RCC/dsp/21/default/AUDI_MIB_DSP.bin.bgz').read_bytes()
assert struct.unpack_from('<I',p)[0]==0xc6000abc and struct.unpack_from('<I',p,4)[0]==len(p)
f=ctypes.CDLL('/opt/homebrew/lib/liblzo2.dylib').lzo1x_decompress_safe
rows=[];decoded=[];at=36
while at+12<=len(p):
 tag,n,dst=struct.unpack_from('<III',p,at)
 if n==0:break
 assert tag in [0xc6c6c600,0xc6c6c601] and at+12+n<=len(p)
 data=p[at+12:at+12+n]
 if tag==0xc6c6c601:
  buf=ctypes.create_string_buffer(0x400000);size=ctypes.c_size_t(len(buf));r=f(data,ctypes.c_size_t(n),buf,ctypes.byref(size),None);assert r==0;data=buf.raw[:size.value]
 rows.append(dict(record_offset=at,tag=hex(tag),stored=n,address=hex(dst),size=len(data),sha256=hashlib.sha256(data).hexdigest()));decoded.append((dst,data));at=(at+12+n+3)&~3
out=A/'artifacts/rcc-dsp-sections';out.mkdir(exist_ok=True)
for (dst,data),row in zip(decoded,rows):(out/(row['address']+'.bin')).write_bytes(data)
low=0x87500180;high=max(dst+len(data) for dst,data in decoded if dst>=low);image=bytearray(high-low)
for dst,data in decoded:
 if dst>=low:image[dst-low:dst-low+len(data)]=data
(out/'memory.bin').write_bytes(image);(out/'records.json').write_text(json.dumps(dict(source_sha256=hashlib.sha256(p).hexdigest(),base=hex(low),end=hex(high),records=rows),indent=2)+'\n')
print('BGZ records',len(rows),'memory',len(image),'bytes')

# Materialize the TI .cinit records separately from their ROM load addresses.
# Destination fields below are data locations; no DSP patch is generated.
cinit=next(data for dst,data in decoded if dst==0x876113c8)
init_out=out/'initialized';init_out.mkdir(exist_ok=True);init_rows=[];at=0
while at+8<=len(cinit):
 n,dst=struct.unpack_from('<II',cinit,at)
 if n==0:break
 assert n<0x100000 and at+8+n<=len(cinit)
 data=cinit[at+8:at+8+n]
 (init_out/(hex(dst)+'.bin')).write_bytes(data)
 init_rows.append(dict(source_offset=at,source_va=hex(0x876113c8+at+8),destination=hex(dst),size=n))
 at=(at+8+n+7)&~7
(init_out/'records.json').write_text(json.dumps(init_rows,indent=2)+'\n')
print('TI cinit',len(init_rows),'records')
