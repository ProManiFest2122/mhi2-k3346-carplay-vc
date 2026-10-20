from pathlib import Path
import subprocess
r=Path(__file__).resolve().parents[1]
template=r/'config/traceConfig.reference.v8h.properties'
expected=r/'config/traceConfig.v8g2.enabled.properties'
awk=r/'deployment/patch-v8i-traceconfig.awk'
p=subprocess.run(['awk','-f',str(awk),str(template)],capture_output=True)
assert p.returncode==0,p.stderr
assert p.stdout==expected.read_bytes(), (len(p.stdout), len(expected.read_bytes()))
p=subprocess.run(['awk','-f',str(awk)],input=b'bad data\n',capture_output=True)
assert p.returncode!=0
print('V8I_DSI_TRACE_SCOPE_TRANSFORM=PASS')
