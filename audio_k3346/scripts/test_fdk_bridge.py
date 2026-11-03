#!/usr/bin/env python3
"""Build/test the K3346 AAC backend using the reference's codec-table ABI."""
import ctypes as C
import json
import random
import struct
import subprocess
from pathlib import Path
import os
import numpy as np

A=Path(__file__).resolve().parents[1]


def main():
    out=A/'artifacts/fdk-decoder-host'
    cmd=['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fPIC','-shared',str(A/'prototype/fdk_aac_bridge.c'),'-I'+str(A/'reference-audio-a63d547/hook/audio')]
    for p in sorted((Path(os.environ.get('MHI2_FDK_SRC', str(A/'deps/fdk-aac')))/'cpp').glob('*/include')):
        cmd+=['-isystem',str(p)]
    cmd += [str(out/'libmhi2_fdk.dylib'),'-o',str(out/'libmhi2_aac_bridge.dylib')]
    subprocess.run(cmd,check=True)
    lib=C.CDLL(str(out/'libmhi2_aac_bridge.dylib'))
    u8=C.POINTER(C.c_ubyte);u32=C.POINTER(C.c_uint32);handle=C.c_void_p
    create=C.CFUNCTYPE(C.c_int,u8,C.c_size_t,C.POINTER(handle),u32,u32,u32)
    decode=C.CFUNCTYPE(C.c_int,handle,u8,C.c_size_t,C.POINTER(C.c_int16),C.c_size_t,u32)
    reset=C.CFUNCTYPE(None,handle);destroy=C.CFUNCTYPE(None,handle)
    class Info(C.Structure):_fields_=[('channels',C.c_uint32),('frames',C.c_uint32),('rate',C.c_uint32),('layout',C.c_uint32)]
    get_info=C.CFUNCTYPE(C.c_int,handle,C.POINTER(Info))
    class Api(C.Structure):_fields_=[('size',C.c_uint32),('version',C.c_uint32),('create',create),('decode',decode),('reset',reset),('destroy',destroy),('info',get_info)]
    lib.spa_aac_codec_api.restype=C.POINTER(Api);api=lib.spa_aac_codec_api().contents
    assert api.size==C.sizeof(Api) and api.version==1
    cookie=bytes.fromhex('0380808022000000048080801440140018000001f4000001f40005808080021190068080800102')
    def new(blob):
        b=(C.c_ubyte*len(blob)).from_buffer_copy(blob);h=handle();ch=C.c_uint32();frames=C.c_uint32();rate=C.c_uint32()
        result=api.create(b,len(blob),C.byref(h),C.byref(ch),C.byref(frames),C.byref(rate))
        if not result:assert [ch.value,frames.value,rate.value]==[2,1024,48000]
        else:assert not h.value
        return result,h
    status,h=new(cookie);assert status==0
    data=(out/'raw-aac-fixture.bin').read_bytes();count=struct.unpack_from('<I',data,8)[0];at=12;packets=[]
    for _ in range(count):
        n=struct.unpack_from('<I',data,at)[0];at+=4;packets.append(data[at:at+n]);at+=n
    try:
        info=Info();assert api.info(h,C.byref(info))==0 and (info.channels,info.frames,info.rate,info.layout)==(2,1024,48000,0)
        p=(C.c_ubyte*len(packets[0])).from_buffer_copy(packets[0]);pcm=(C.c_int16*2048)();frames=C.c_uint32(999)
        assert api.decode(h,p,len(p),pcm,2047,C.byref(frames))==9 and frames.value==0
        def run():
            rows=[]
            for packet in packets:
                b=(C.c_ubyte*len(packet)).from_buffer_copy(packet)
                assert api.decode(h,b,len(packet),pcm,2048,C.byref(frames))==0 and frames.value==1024
                rows.append(np.ctypeslib.as_array(pcm).copy())
            return np.concatenate(rows)
        first=run();api.reset(h);assert np.array_equal(first,run())
        assert api.decode(h,None,0,None,0,C.byref(frames))==0 and frames.value==0
        assert np.array_equal(first,run())
        api.reset(h)
        bad=(C.c_ubyte*16)(*[255]*16)
        assert api.decode(h,bad,16,pcm,2048,C.byref(frames))==8 and frames.value==0
        assert api.decode(h,p,len(p),pcm,2048,C.byref(frames))==8 and frames.value==0
        api.reset(h);assert np.array_equal(first,run())
    finally:api.destroy(h)
    malformed=[cookie[:i] for i in range(len(cookie))]
    malformed += [b'\x05\x80\x80\x80\x80\x00',b'\x05\x02\x11\x90'*2,bytes.fromhex('05021310'),bytes.fromhex('0303000040')]
    rng=random.Random(3346)
    malformed += [bytes(rng.randrange(256) for _ in range(rng.randrange(128))) for _ in range(4000)]
    for blob in malformed:
        status,d=new(blob)
        if d.value:api.destroy(d)
        assert status!=0
    fnv=2166136261
    for byte in first.astype('<i2').tobytes():fnv=((fnv^byte)*16777619)&0xffffffff
    assert fnv==json.loads((out/'analysis.json').read_text())['pcm_fnv1a32']
    result=dict(passed=True,packets=count,malformed_cookies_rejected=len(malformed),capacity_rejection_atomic=True,reset_exact=True,zero_length_reset_exact=True,decode_error_latched_until_reset=True,reference_codec_table_abi=1,pcm_fnv1a32=fnv,scope='Host bridge only; no live receiver/switching/native bridge proof')
    (out/'bridge-analysis.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))


if __name__=='__main__':main()
