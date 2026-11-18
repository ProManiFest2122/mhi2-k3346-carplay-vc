#!/usr/bin/env python3
"""Extract the exact stock K3346 wrapped RCC IFS; read-only source, no flashing."""
import ctypes,hashlib,json,struct,sys,stat
from pathlib import Path
AUDIT=Path(__file__).resolve().parents[1];ROOT=AUDIT.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
def inspect_imagefs(raw):
 assert raw[:8]==b'imagefs\x04'
 assert struct.unpack_from('<I',raw,8)[0]==len(raw)
 assert sum(v[0] for v in struct.iter_unpack('<I',raw))&0xffffffff==0
 pos=struct.unpack_from('<I',raw,16)[0];entries=[]
 while pos+2<len(raw):
  size=struct.unpack_from('<H',raw,pos)[0]
  if not size:break
  assert size>=24 and size%4==0
  inode,mode=struct.unpack_from('<II',raw,pos+4)
  if inode and stat.S_ISREG(mode):
   offset,length=struct.unpack_from('<II',raw,pos+24)
   name=raw[pos+32:pos+size].split(b'\0')[0].decode()
   assert not Path(name).is_absolute() and '..' not in Path(name).parts
   assert offset+length<=len(raw)-4
   entries.append(dict(path=name,offset=offset,size=length,sha256=hashlib.sha256(raw[offset:offset+length]).hexdigest()))
  pos+=size
 return entries

def extract_selected(raw,entries,dest):
 # Full manifest retains duplicate namespace entries; extract only regular audio evidence.
 wanted={'usr/apps/MIBRoot','bin/AudioProcess','etc/MIB_AudioTable_AU_G22.xml','lib/libSPC_configuration.so','etc/srv-starter.cfg'}
 for e in entries:
  if e['path'] not in wanted:continue
  target=dest/e['path'];target.parent.mkdir(parents=True,exist_ok=True)
  assert not target.exists(),e['path']
  target.write_bytes(raw[e['offset']:e['offset']+e['size']])

def main():
 source=ROOT/'runtime/k3346-build/firmware/RCC/ifs-root/21/default/ifs-root.ifs'
 p=source.read_bytes();assert p[8:12]==bytes.fromhex('eb7eff00')
 stage2='--stage2' in sys.argv
 header=0x660000 if stage2 else 8
 assert p[header:header+6]==bytes.fromhex('eb7eff000100')
 off=header+struct.unpack_from('<I',p,header+32)[0];start=off;chunks=[];blocks=[]
 f=ctypes.CDLL('/opt/homebrew/lib/liblzo2.dylib').lzo1x_decompress_safe
 while off+2<=len(p):
  n=int.from_bytes(p[off:off+2],'big')
  if not n:break
  assert off+2+n<=len(p)
  b=ctypes.create_string_buffer(65536);size=ctypes.c_size_t(len(b))
  status=f(p[off+2:off+2+n],ctypes.c_size_t(n),b,ctypes.byref(size),None)
  assert status==0,(hex(off),n,status,size.value)
  chunks.append(b.raw[:size.value]);blocks.append(dict(offset=off,stored=n+2,decoded=size.value));off+=n+2
 raw=b''.join(chunks);entries=inspect_imagefs(raw);a=AUDIT/'artifacts'
 prefix='rcc-k3346-stage2' if stage2 else 'rcc-k3346'
 dest=a/(prefix+'-extracted');assert not dest.exists()
 (a/(prefix+'-imagefs.bin')).write_bytes(raw);extract_selected(raw,entries,dest)
 (a/(prefix+'-ifs.json')).write_text(json.dumps(dict(source=str(source),source_sha256=hashlib.sha256(p).hexdigest(),compression_start=start,blocks=blocks,entries=entries),indent=2)+'\n')
 print('Validated RCC imagefs',len(raw),'bytes;',len(blocks),'LZO1X blocks;',len(entries),'entries')
if __name__=='__main__':main()
