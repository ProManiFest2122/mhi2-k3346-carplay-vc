#!/usr/bin/env python3
"""Build an isolated decoder-only experiment from pinned upstream source lists.

No encoder, installation or vehicle payload. Upstream NOTICE must accompany
any future distribution; this script currently builds local research artifacts.
"""
import argparse
import concurrent.futures
import json
import hashlib
import os
import re
import subprocess
from pathlib import Path

A = Path(__file__).resolve().parents[1]
COMMIT = 'c41c3c7516ea8d0d78a53b96b30bed3a29745cc6'


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('target', choices=['host', 'qnx'])
    ap.add_argument('--source', type=Path, default=Path(os.environ.get('MHI2_FDK_SRC', str(A/'deps/fdk-aac'))))
    args = ap.parse_args()
    src = args.source.resolve()
    actual = subprocess.check_output(['git', '-C', str(src), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != COMMIT:
        raise ValueError('Unexpected FDK source revision')
    if subprocess.check_output(['git', '-C', str(src), 'status', '--porcelain', '--untracked-files=no'], text=True).strip():
        raise ValueError('Modified upstream sources are not accepted')
    epoch = subprocess.check_output(['git', '-C', str(src), 'show', '-s', '--format=%ct', 'HEAD'], text=True).strip()
    build_env = dict(os.environ, SOURCE_DATE_EPOCH=epoch)
    text = (src / 'Makefile.am').read_text().replace('\\\n', ' ')
    names = ['AACDEC', 'ARITHCODING', 'DRCDEC', 'MPEGTPDEC', 'SACDEC', 'SBRDEC', 'PCMUTILS', 'FDK', 'SYS']
    sources = []
    for name in names:
        match = re.search(r'^' + name + r'_SRC\s*=([^\n]+)', text, re.M)
        if not match:
            raise ValueError('Missing documented source group ' + name)
        sources.extend(src / item for item in match[1].split())
    out = A / 'artifacts' / ('fdk-decoder-' + args.target)
    out.mkdir(parents=True, exist_ok=True)
    flags = ['-O2', '-fPIC', '-ffunction-sections', '-fdata-sections', '-fno-exceptions', '-fno-rtti', '-std=c++11']
    flags += ['-I' + str(p) for p in sorted((src / 'cpp').glob('*/include'))]
    if args.target == 'host':
        compiler = 'c++'
    else:
        compiler = str(Path(os.environ.get('MHI2_TOOLCHAIN', str(A/'deps/xpack-arm-none-eabi-gcc-13.3.1-1.1')))/'bin/arm-none-eabi-g++')
        flags[0] = '-Os'
        headers = Path(os.environ.get('MHI2_QNX_HEADERS', str(A/'deps/openqnx/trunk')))
        flags += ['-mfloat-abi=softfp', '-mfpu=neon', '-mcpu=cortex-a9', '-marm', '-mno-unaligned-access', '-fno-builtin', '-nostdlib', '-D__QNXNTO__', '-D__QNX__', '-D__ARM__', '-D__LITTLEENDIAN__', '-D_QNX_SOURCE', '-I' + str(headers / 'lib/c/public'), '-I' + str(headers / 'services/system/public')]

    def compile_one(item):
        obj = out / ('_'.join(item.relative_to(src).parts) + '.o')
        command = [compiler, *flags, '-c', str(item), '-o', str(obj)]
        result = subprocess.run(command, capture_output=True, text=True, env=build_env)
        (out / (obj.name + '.log')).write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(str(item) + '\n' + result.stderr[-3000:])
        return str(obj)

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        objects = list(pool.map(compile_one, sources))
    library = out / ('libmhi2_fdk.dylib' if args.target == 'host' else 'libmhi2_fdk.so')
    command = [compiler, *flags, '-shared', '-o', str(library), *objects]
    if args.target == 'qnx':
        exports = out / 'exports.map'
        exports.write_text('{ global: aacDecoder_Open; aacDecoder_Close; aacDecoder_ConfigRaw; aacDecoder_Fill; aacDecoder_DecodeFrame; aacDecoder_GetStreamInfo; aacDecoder_SetParam; local: *; };\n')
        command += ['-Wl,-soname,libmhi2_fdk.so,--gc-sections,--strip-all,--version-script=' + str(exports), str(A / 'artifacts/native-type103/libc-extra.so'), '-lgcc']
    subprocess.run(command, check=True, env=build_env)
    (out / 'NOTICE').write_bytes((src / 'NOTICE').read_bytes())
    (out / 'provenance.json').write_text(json.dumps(dict(source='https://github.com/mstorsjo/fdk-aac', commit=COMMIT, source_date_epoch=epoch, groups=names, source_count=len(sources), target=args.target, compiler=compiler, flags=flags, library_sha256=hashlib.sha256(library.read_bytes()).hexdigest(), scope='Decoder experiment; no Cinemo dependency; not a vehicle package'), indent=2) + '\n')
    print(library, len(sources), 'sources')


if __name__ == '__main__':
    main()
