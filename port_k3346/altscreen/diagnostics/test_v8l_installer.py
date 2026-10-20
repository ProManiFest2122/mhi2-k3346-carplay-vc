#!/usr/bin/env python3
"""Extra negative/regression tests for v8k installer fixes. No vehicle access."""
from pathlib import Path
from zipfile import ZipFile
import os, subprocess, tempfile, shutil, stat, json, hashlib
ROOT=Path(os.environ.get('V8L_SD_ROOT','/mnt/data/v8l_work/base_install'))
PKG='mib2high-carplay-rgi-v8l-simple'
CAR=Path('/mnt/data/altscreen_inputs/v8i-full-diagnostic-2026-10-10.zip')
with ZipFile(CAR) as z:
    p='v8i-full-diagnostic/collected/run-01/firmware/files/'
    conf={name:z.read(p+prefix+name) for prefix,name in [('8-','bundles_dsitracer_enabled.properties'),('11-','traceConfig.properties'),('36-','smartphone_integrator.json')]}

def fixture():
    root=Path(tempfile.mkdtemp(prefix='v8k-fixed-negative-'))
    sd,app,system=root/'sd',root/'app',root/'system'
    shutil.copytree(ROOT,sd)
    lsd=app/'eso/hmi/lsd';lsd.mkdir(parents=True)
    (app/'root/hooks').mkdir(parents=True)
    (lsd/'jars').mkdir(parents=True)
    (lsd/'DSITracer.jar').write_bytes(b'STOCK DSITracer JAR MOCK')
    (lsd/'bundles_dsitracer_enabled.properties').write_bytes(conf['bundles_dsitracer_enabled.properties'])
    tr=lsd/'traceConfig.properties'
    old=conf['traceConfig.properties'].decode()
    # Missing one class, alternate value for another, and duplicate key.
    new='\n'.join(line for line in old.splitlines() if 'org.dsi.ifc.kombisync.DSIKombiSync=' not in line)
    new=new.replace('org.dsi.ifc.map.DSIMapViewerControl=full','org.dsi.ifc.map.DSIMapViewerControl=off')
    new=new.replace('use.appender=','use.appender=diagnostic\n#use.appender=')
    new+='\ncustom.unrelated=test-value\n'
    tr.write_text(new)
    other=app/'root/vc_render';other.write_bytes(b'ORIGINAL RENDERER');other.chmod(0o700)
    script=app/'root/mib2high-lvds-control.sh';script.write_bytes(b'ORIGINAL SCRIPT');script.chmod(0o741)
    marker=app/'carplay_altscreen';marker.touch();marker.chmod(0o1700)
    tr.chmod(0o600)
    jar=lsd/'jars/mib2high-carplay-rgi.jar';jar.write_bytes(b'ORIGINAL JAR');jar.chmod(0o640)
    smart=system/'etc/eso/production/smartphone_integrator.json';smart.parent.mkdir(parents=True);smart.write_bytes(conf['smartphone_integrator.json']);smart.chmod(0o600)
    original={str(p.relative_to(root)):(p.read_bytes(),stat.S_IMODE(p.stat().st_mode)) for p in list(app.rglob('*'))+list(system.rglob('*')) if p.is_file()}
    tools=root/'fakebin';tools.mkdir()
    for name in ['sloginfo','pidin','netstat']:
        f=tools/name;f.write_text('#!/bin/sh\necho "MOCK '+name+' TRACE"\n');f.chmod(0o755)
    env={**os.environ,'MIB_SD':str(sd),'MIB_APP':str(app),'MIB_SYSTEM':str(system),'PATH':str(tools)+':'+os.environ['PATH']}
    return root,sd,app,system,env,original

def run(sd,env):
    r=subprocess.run(['sh',str(sd/'mod/custom.sh')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env,timeout=30)
    return r.returncode,r.stdout

def compare(root,app,system,original):
    now={str(p.relative_to(root)):(p.read_bytes(),stat.S_IMODE(p.stat().st_mode)) for p in list(app.rglob('*'))+list(system.rglob('*')) if p.is_file()}
    assert now==original,[(k,original.get(k),now.get(k)) for k in sorted(set(now)|set(original)) if now.get(k)!=original.get(k)]

results=[]
root,sd,app,system,env,original=fixture()
try:
    code,out=run(sd,env)
    assert code==0 and 'INSTALL_RESULT=PASS' in out,repr(out)
    config=(app/'eso/hmi/lsd/traceConfig.properties').read_text()
    expected=['use.appender=telnet,bluepirat','org.dsi.ifc.keypanel.DSIKeyPanel=full', 'org.dsi.ifc.map.DSIMapViewerZoomEngine=full', 'org.dsi.ifc.komoview.DSIKOMOView=full', 'org.dsi.ifc.map.DSIMapViewerControl=full', 'org.dsi.ifc.kombisync.DSIKombiSync=full']
    for line in expected:assert config.splitlines().count(line)==1,line
    assert 'custom.unrelated=test-value' in config
    assert (app/'root/vc_render').stat().st_mode & 0o777==0o755
    trlog=Path('/tmp/v8l-dsitracer-1798-00.log');trlog.write_text('MOCK DSITracer captured event\n')
    code,out=run(sd,env)
    assert code==0 and 'COLLECT_RESULT=PASS' in out,out
    assert (sd/'mod'/PKG/'collected/run-01/logs/v8l-dsitracer-1798-00.log').is_file()
    (sd/'mod'/PKG/'ROLLBACK').touch()
    code,out=run(sd,env)
    assert code==0 and 'ROLLBACK_RESULT=PASS' in out,out
    compare(root,app,system,original)
    results.append('missing+altered+duplicate DSI filters -> install PASS, collect PASS, rollback exact bytes+modes PASS')
finally: shutil.rmtree(root)

# Mid-install forced failure with special modes on markers and original files.
root,sd,app,system,env,original=fixture()
try:
    script=sd/'mod/custom.sh';s=script.read_text()
    needle='install_file "$PAY/$n" "$dest" `modeof "$n"` || fail "install failed $n"'
    assert needle in s
    s=s.replace(needle,'''if [ "$n" = mib2high-lvds-control.sh ]; then false; else install_file "$PAY/$n" "$dest" `modeof "$n"`; fi || fail "install failed $n"''')
    script.write_text(s)
    code,out=run(sd,env)
    assert code!=0 and 'AUTO_ROLLBACK_RESULT=PASS' in out,out
    compare(root,app,system,original)
    results.append('mid-install failure -> atomic rollback original bytes+modes PASS')
finally: shutil.rmtree(root)

# A minimal config must not halt install; default every target scope.
root,sd,app,system,env,original=fixture()
try:
    (app/'eso/hmi/lsd/traceConfig.properties').write_text('unrelated=keep\n')
    code,out=run(sd,env)
    assert code==0 and 'INSTALL_RESULT=PASS' in out,out
    txt=(app/'eso/hmi/lsd/traceConfig.properties').read_text()
    for key in ['use.appender=telnet,bluepirat','org.dsi.ifc.kombisync.DSIKombiSync=full','org.dsi.ifc.komoview.DSIKOMOView=full']:assert key in txt
    results.append('minimal tracer config -> install PASS')
finally: shutil.rmtree(root)

trlog.unlink(missing_ok=True)
print(json.dumps({'result':'PASS','scenarios':results},indent=2))
