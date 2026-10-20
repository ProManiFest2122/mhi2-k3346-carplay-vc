#!/usr/bin/env python3
from pathlib import Path
from zipfile import ZipFile
import os, subprocess, tempfile, shutil, json, hashlib, uuid
ROOT=Path(__file__).resolve().parents[2]/'sd-v8k'
PACKAGE='mib2high-carplay-rgi-v8k-simple'
CAR=Path('/mnt/data/altscreen_inputs/v8i-full-diagnostic-2026-10-10.zip')
assert ROOT.exists()
with ZipFile(CAR) as z:
    base='v8i-full-diagnostic/collected/run-01/firmware/files/'
    car={'bundles_dsitracer_enabled.properties':z.read(base+'8-bundles_dsitracer_enabled.properties'),
         'traceConfig.properties':z.read(base+'11-traceConfig.properties'),
         'smartphone_integrator.json':z.read(base+'36-smartphone_integrator.json')}

def run(sd,env):
    r=subprocess.run(['sh',str(sd/'mod/custom.sh')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env,timeout=35)
    return r.returncode,r.stdout

def fixture(existing):
    root=Path(tempfile.mkdtemp(prefix='v8k-test-'))
    sd,app,sys=root/'sd',root/'app',root/'system'
    shutil.copytree(ROOT,sd)
    lsd=app/'eso/hmi/lsd';lsd.mkdir(parents=True)
    (app/'root').mkdir(parents=True)
    (lsd/'DSITracer.jar').write_bytes(b'QNX TEST TRACER')
    for name in ['bundles_dsitracer_enabled.properties','traceConfig.properties']:(lsd/name).write_bytes(car[name])
    smart=sys/'etc/eso/production/smartphone_integrator.json';smart.parent.mkdir(parents=True);smart.write_bytes(car['smartphone_integrator.json'])
    if existing:
        (app/'root/hooks').mkdir(parents=True)
        (lsd/'jars').mkdir(parents=True)
        for p in [app/'root/hooks/libmib2high-carplay-rgi.so',lsd/'jars/mib2high-carplay-rgi.jar',app/'root/vc_render',app/'root/mib2high-lvds-control.sh',app/'root/mib2high-lvds-profile.conf',app/'root/mib2high-lvds-viewareas.conf']:
            p.write_bytes(b'EXISTING V8I FILE')
        (app/'carplay_altscreen').touch()
        (lsd/'dsitracer_activated').touch()
    orig={str(p.relative_to(root)):p.read_bytes() for tree in [app,sys] for p in tree.rglob('*') if p.is_file()}
    tools=root/'fakebin';tools.mkdir()
    for name in ['sloginfo','pidin','netstat']:
        stub=tools/name
        stub.write_text('#!/bin/sh\necho "'+name+' MOCK DSITRACER EVENT setZoomArea K2161"\n')
        stub.chmod(0o755)
    env={**os.environ,'MIB_SD':str(sd),'MIB_APP':str(app),'MIB_SYSTEM':str(sys),'PATH':str(tools)+':'+os.environ['PATH']}
    return root,sd,app,sys,env,orig

# Native logs on the MMI live under /tmp. Provide a test-only file matching
# the observed car path patterns, then remove it on exit.
mock_log=Path('/tmp/v8k-test-gauge-'+uuid.uuid4().hex+'.log')
mock_log.write_text('V8K MOCK GAUGE TRACE LOG\n')
checks=[]
for existing in (False,True):
    root,sd,app,sys,env,orig=fixture(existing)
    status,log=run(sd,env)
    assert status==0 and 'INSTALL_RESULT=PASS' in log,(existing,status,log)
    pkg=sd/'mod'/PACKAGE
    assert (pkg/'installed.ok').is_file()
    for name in ['libmib2high-carplay-rgi.so','mib2high-carplay-rgi.jar','vc_render','mib2high-lvds-control.sh','mib2high-lvds-profile.conf','mib2high-lvds-viewareas.conf','v8k-dsitracer-capture']:
        dest=app/('root/hooks/'+name if name.endswith('.so') else 'eso/hmi/lsd/jars/'+name if name.endswith('.jar') else 'root/'+name)
        assert dest.read_bytes()==(pkg/'payload'/name).read_bytes(),name
    assert (app/'root/mib2high-lvds-profile.conf').read_text()=='1440 542 30 0 0 0 1440 542\n'
    assert 'physical 200 75' in (app/'root/mib2high-lvds-viewareas.conf').read_text()
    smart=(sys/'etc/eso/production/smartphone_integrator.json').read_text()
    assert smart.count('LD_PRELOAD=/mnt/app/root/hooks/libmib2high-carplay-rgi.so')==1
    assert 'MIB2HighCarPlayRGI=org.mib2high.carplay.rgi.Activator' in (app/'eso/hmi/lsd/bundles_dsitracer_enabled.properties').read_text()
    # Provide deterministic synthetic buffered tracer data, since the real QNX socket
    # is tested separately and cannot run inside Linux install sandbox.
    trace_mock=Path('/tmp/v8k-dsitracer-00.log')
    trace_mock.write_text('MOCK DSITracer DSIKombiSync screenFormat=1\n')
    java_mock=Path('/tmp/v8k-rgi-vc-java.log')
    java_mock.write_text('MOCK VC_TRACE VC_SET_FAVORED_BEGIN mode=1\n')
    status2,log2=run(sd,env)
    assert status2==0 and 'COLLECT_RESULT=PASS' in log2,(existing,status2,log2)
    d=pkg/'collected/run-01'
    assert 'MOCK DSITRACER' in (d/'trace/sloginfo-raw.txt').read_text()
    assert 'DSIKombiSync' in (d/'logs/v8k-dsitracer-00.log').read_text()
    assert 'VC_SET_FAVORED_BEGIN' in (d/'logs/v8k-rgi-vc-java.log').read_text()
    assert 'org.dsi.ifc.kombisync.DSIKombiSync=full' in (app/'eso/hmi/lsd/traceConfig.properties').read_text()
    assert 'V8K MOCK GAUGE TRACE LOG' in (d/'logs'/mock_log.name).read_text()
    assert (d/'live/libmib2high-carplay-rgi.so').read_bytes()==(pkg/'payload/libmib2high-carplay-rgi.so').read_bytes()
    (pkg/'ROLLBACK').touch()
    status3,log3=run(sd,env)
    assert status3==0 and 'ROLLBACK_RESULT=PASS' in log3,(existing,status3,log3)
    after={str(p.relative_to(root)):p.read_bytes() for tree in [app,sys] for p in tree.rglob('*') if p.is_file()}
    assert after==orig,(existing,list(after.keys()),list(orig.keys()))
    status4,log4=run(sd,env)
    assert status4==0 and 'ALREADY_ROLLED_BACK' in log4,(existing,status4,log4)
    checks.append({'existing_runtime':existing,'install':'PASS','collect':'PASS','rollback':'PASS','reinstall_blocked_after_rollback':'PASS'})
    shutil.rmtree(root)

# Unexpected actual QNX file-copy failure must not print a false collect PASS.
root,sd,app,sys,env,orig=fixture(True)
status,log=run(sd,env)
assert status==0 and 'INSTALL_RESULT=PASS' in log
# Deliberate failed command in collector sandbox: simulated missing sloginfo
fake=Path(env['PATH'].split(':')[0])/'sloginfo'
fake.write_text('#!/bin/sh\nexit 1\n');fake.chmod(0o755)
status,log=run(sd,env)
assert status==0 and 'COLLECT_RESULT=PARTIAL' in log,log
assert 'SLOGINFO_FAILED' in (sd/'mod'/PACKAGE/'collected/run-01/trace/collection-errors.txt').read_text()
shutil.rmtree(root)
mock_log.unlink()
Path("/tmp/v8k-dsitracer-00.log").unlink(missing_ok=True)
Path("/tmp/v8k-rgi-vc-java.log").unlink(missing_ok=True)

script=(ROOT/'mod/custom.sh').read_text()
assert 'tr -d' not in script and 'cmp ' not in '\n'.join(x for x in script.splitlines() if not x.lstrip().startswith('#'))
assert 'cksum ' not in '\n'.join(x for x in script.splitlines() if not x.lstrip().startswith('#'))
assert script.find('if [ -f "$ROLLBACK" ]')<script.find('elif [ -f "$INSTALLED" ]')
print(json.dumps({'test':'V8K_INSTALLER_MOCK_PASS','scenarios':checks,'partial_error_reporting':'PASS'},indent=2))

# Test-only fault injection into a temporary COPY of the shell script, not any
# compiled binary or release payload. Force failure mid-install and test restore.
root,sd,app,sys,env,orig=fixture(True)
shell=sd/'mod/custom.sh'
s=shell.read_text()
needle='install_file "$PAY/$n" "$dest" `modeof "$n"` || fail "install failed $n"'
assert needle in s
s=s.replace(needle, '''if [ "$n" = mib2high-lvds-control.sh ]; then false; else install_file "$PAY/$n" "$dest" `modeof "$n"`; fi || fail "install failed $n"''')
shell.write_text(s)
status,log=run(sd,env)
assert status!=0 and 'AUTO_ROLLBACK_RESULT=PASS' in log,log
after={str(p.relative_to(root)):p.read_bytes() for tree in [app,sys] for p in tree.rglob('*') if p.is_file()}
assert orig==after
assert not (sd/'mod'/PACKAGE/'installed.ok').exists()
shutil.rmtree(root)
print('V8K_MID_INSTALL_FAILURE_ROLLBACK=PASS')
