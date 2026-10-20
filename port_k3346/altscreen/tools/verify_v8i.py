from pathlib import Path
import subprocess,re,hashlib,json
D=Path(__file__).resolve().parents[1]
def sha(f): return hashlib.sha256(f.read_bytes()).hexdigest()
def readelf(flags,p):return subprocess.check_output(['/opt/qnx49-wrap/arm-unknown-nto-qnx6.5.0eabi-readelf',flags,str(p)],text=True)
cur=D/'output/libmib2high-carplay-rgi.so'
assert cur.is_file() and cur.read_bytes()[:4]==b'\x7fELF'
old=D/'reference/v8g-known-good.so'
assert sha(old)=='20ed396c43e942a50fa2165c3d243b86135126f3b53e31a971809a3cc305f3fe'
s0=readelf('-Ws',old);s1=readelf('-Ws',cur)
parse=lambda s:sorted(re.findall(r'^\s*\d+:\s+[0-9a-f]+\s+\d+\s+FUNC\s+GLOBAL\s+DEFAULT\s+\d+\s+(\S+)',s,re.M))
a,b=parse(s0),parse(s1)
assert len(set(a))==131 and a==b,(len(a),len(b),set(a)^set(b))
needs=lambda s:sorted(re.findall(r'\(NEEDED\).*?\[(.*?)\]',s))
assert needs(readelf('-d',old))==needs(readelf('-d',cur))
assert 'Machine:                           ARM' in readelf('-h',cur)
assert 'Version5 EABI' in readelf('-h',cur)
config=(D/'config/bundles_dsitracer_enabled.v8i.merged.properties').read_text()
assert 'DSITracer,FwUtilServices,Profiling,Tracing,ATIPImpl,MIB2HighCarPlayRGI' in config
assert 'MIB2HighCarPlayRGI=org.mib2high.carplay.rgi.Activator' in config
report={'status':'PASS_OFFLINE_NOT_CAR_TESTED','sha256':sha(cur),'bytes':cur.stat().st_size,'same_131_exports':True,'same_dependencies':True,'rgiregistry':sha(D/'config/bundles_dsitracer_enabled.v8i.merged.properties')}
(D/'output/V8I_BUILD_VERIFICATION.json').write_text(json.dumps(report,indent=2)+'\n')
print('V8I_QNX_SOURCE_BUILD=PASS SHA256='+sha(cur))
