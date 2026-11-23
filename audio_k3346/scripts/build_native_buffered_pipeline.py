#!/usr/bin/env python3
"""Build the emulator-only APAP/codec/ring/NVIDIA harness with stock QNX libs."""
from pathlib import Path
import json, subprocess, sys, zlib, struct, re
A=Path(__file__).resolve().parents[1]; W=A.parent.parent
sys.path[:0]=[str(W/'tools'),str(W/'runtime/qemu-mhi2/tools/mhi2/glforward'),str(W/'runtime/qemu-mhi2/tools/mhi2')]
from mhi2_ifs import read_imagefs
import os
STOCK=os.environ.get('PIPELINE_VARIANT')=='nv_stock'
DUMP=os.environ.get('PIPELINE_VARIANT')=='aac_dump'
REPLAY=os.environ.get('PIPELINE_VARIANT')=='nv_replay'
NV=os.environ.get('PIPELINE_VARIANT') in ('nv_hook','nv_stock')
O=A/('artifacts/native-nv-stock' if STOCK else 'artifacts/native-aac-dump' if DUMP else 'artifacts/native-nv-replay' if REPLAY else 'artifacts/native-nv-hook' if NV else 'artifacts/native-buffered-pipeline');O.mkdir(exist_ok=True)
R=A/'reference-audio-a63d547';H=Path(os.environ.get('MHI2_QNX_HEADERS', str(A/'deps/openqnx/trunk')))
TC=Path(os.environ.get('MHI2_TOOLCHAIN', str(A/'deps/xpack-arm-none-eabi-gcc-13.3.1-1.1')))/'bin/arm-none-eabi'
def run(args):return subprocess.check_output(list(map(str,args)),stderr=subprocess.STDOUT,text=True)
def main():
    manifest=json.loads((W/'runtime/k3346-build/media/base/manifest.json').read_text())
    _,offset,capacity=manifest['partitions']['KERNEL_PRIMARY']
    nor=(W/'runtime/k3346-build/media/base/nor.bin').read_bytes()
    kernel=zlib.decompress(nor[offset+2048:offset+capacity])
    base=8+struct.unpack_from('<I',kernel,40)[0]
    size=struct.unpack_from('<I',kernel,base+8)[0]
    raw=kernel[base:base+size]
    libs={}
    for e in read_imagefs(raw):
        name=Path(e['path']).name
        if name in ('libc.so.3','libm.so.2') and e['kind']=='file':
            p=O/name;p.write_bytes(raw[e['offset']:e['offset']+e['size']]);libs[name]=p
    assert len(libs)==2,libs
    # Stock QNX ELFs retain PT_DYNAMIC but remove section tables needed by GNU ld.
    # Link-only views export ONLY verified real symbols. Never ship these files.
    views={}
    for name,p in libs.items():
        table=subprocess.run([str(TC)+'-readelf','-W','--use-dynamic','-s',str(p)],capture_output=True,text=True,check=True).stdout
        asm=['.text']
        for line in table.splitlines():
            fields=line.split()
            if len(fields)!=8 or fields[4] not in ('GLOBAL','WEAK') or fields[6]=='UND':continue
            symbol=fields[7]
            if not re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*',symbol):continue
            kind=fields[3]
            if kind not in ('FUNC','OBJECT'):continue
            asm += ['.data' if kind=='OBJECT' else '.text','.balign 4','.global '+symbol,
                    '.type '+symbol+', %'+('object' if kind=='OBJECT' else 'function'),symbol+':',
                    '.space '+str(max(4,int(fields[2]))) if kind=='OBJECT' else 'bx lr']
        src=O/(name+'-LINK-ONLY.S');src.write_text('\n'.join(asm)+'\n')
        view=O/(name+'-LINK-ONLY.so')
        run([str(TC)+'-gcc','-marm','-nostdlib','-fPIC','-shared','-Wl,-soname,'+name,src,'-o',view]);views[name]=view
    flags=['-mfloat-abi=softfp','-mfpu=neon','-mcpu=cortex-a9','-marm','-mno-unaligned-access','-fno-builtin','-nostdlib','-O2','-ffp-contract=off','-std=gnu99','-fPIC','-ffunction-sections','-fdata-sections','-D__QNXNTO__','-D__QNX__','-D__ARM__','-D__LITTLEENDIAN__','-D_QNX_SOURCE','-DMHI2_PIPELINE_NATIVE']
    if NV:flags+=['-DMHI2_NV_SIX_CHANNEL_QUALIFIED=1']
    if STOCK:flags+=['-DMHI2_NV_STOCK_BRIDGE','-DSPA_NV_STOCK_DEVICE="MPL1"']
    for p in [A/'prototype/qnx-compat',H/'lib/c/public',H/'services/system/public',R/'tests/mocks',R/'hook',R/'hook/audio',A/'prototype']:flags+=['-I'+str(p)]
    if DUMP:
        sources=[A/'prototype/aac_dump.c']
    elif REPLAY:
        sources=[A/'prototype/nv_replay.c',A/'prototype/nvidia_pcm_sink.c']
    else:
      sources=[A/'prototype'/n for n in [('test_nv_hook_detached.c' if NV else 'test_buffered_nvidia_pipeline.c'),'nvidia_pcm_sink.c']+([] if NV else ['nvidia_receiver_pump.c'])]
    if not (REPLAY or DUMP):
      sources += [R/'hook/audio'/n for n in ['stereo_upmix.c','soundcheck.c','scene_engine_math.c','scene_clusters.c','scene_engine.c','scene_mask.c','scene_mask_worker.c','spatial_downmix.c','apap_lifecycle.c','apap_crypto.c']]
    if not (REPLAY or DUMP):
      sources += [R/'hook/framework/iap2_protocol.c']
    objects=[]
    for src in sources:
        obj=O/(src.stem+'.o');run([str(TC)+'-gcc',*flags,'-c',src,'-o',obj]);objects.append(obj)
    start=O/'start.c'
    start.write_text('extern int main(int,char **); extern void exit(int); void _start(void){char *args[]={"replay","/mnt/audio-pipeline/expected-writes.bin",0};exit(main(2,args));}\n' if REPLAY else 'extern int main(int,char **); extern void exit(int); void _start(void){char *args[]={"aacdump","/mnt/audio-pipeline/raw-aac-fixture.bin",0};exit(main(2,args));}\n' if DUMP else 'extern int main(int,char **); extern void exit(int); void _start(void){char *args[]={"pipeline","/mnt/audio-pipeline/frames.bin","/mnt/audio-pipeline/raw-aac-fixture.bin",0};exit(main(3,args));}\n')
    # test source deliberately undefines main after including baseline tests.
    obj=O/'start.o';run([str(TC)+'-gcc',*flags,'-c',start,'-o',obj]);objects.append(obj)
    target=O/('native-nv-stock' if STOCK else 'native-aac-dump' if DUMP else 'native-nv-replay' if REPLAY else 'native-nv-hook-detached' if NV else 'native-buffered-pipeline')
    deps=[A/'artifacts/fdk-decoder-qnx/libmhi2_aac.so',A/'artifacts/fdk-decoder-qnx/libmhi2_fdk.so',views['libc.so.3'],views['libm.so.2']] if DUMP else [A/'artifacts/native-nvidia-sink/nvaudio-link-stub.so',views['libc.so.3'],views['libm.so.2']] if REPLAY else [A/'artifacts/native-type103/libmhi2_apac.so',A/'artifacts/fdk-decoder-qnx/libmhi2_aac.so',A/'artifacts/fdk-decoder-qnx/libmhi2_fdk.so',A/'artifacts/native-nvidia-sink/nvaudio-link-stub.so',views['libc.so.3'],views['libm.so.2']]
    run([str(TC)+'-gcc',*flags,'-Wl,--gc-sections,--allow-shlib-undefined,--dynamic-linker=/usr/lib/ldqnx.so.2,-e,_start','-o',target,*objects,*deps,'-lgcc'])
    dynamic=run([str(TC)+'-readelf','-d',target]);relocs=run([str(TC)+'-readelf','-r',target])
    assert 'R_ARM_COPY' not in relocs,relocs
    (O/'elf-audit.txt').write_text(dynamic+'\n'+relocs)
    print(dynamic);print('Native buffered pipeline build PASS; no COPY relocations')
if __name__=='__main__':
    try:main()
    except subprocess.CalledProcessError as e:print(e.output);raise SystemExit(e.returncode)
