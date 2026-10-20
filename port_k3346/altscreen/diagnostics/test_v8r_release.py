#!/usr/bin/env python3
"""v8R offline packaging and recovery tests; never run on an MMI."""
from pathlib import Path
import tempfile, hashlib, shutil, zipfile, subprocess, os, stat, json
W=Path('/mnt/data/v8r_work')
S=W/'v8r_source'
PK='mib2high-carplay-rgi-v8r-simple'
SD=W/'v8r_install'
BASE=Path('/mnt/data/K2161_V8P_V8N_GEOMETRY_GAUGE_NO_SHOWUI_INSTALL_COLLECT_2.zip')
FW=Path('/mnt/data/v8i-full-diagnostic-2026-10-10.zip')
EXPECT={0:(370,72,700,378),1:(370,72,700,378),2:(490,72,460,378),3:(490,72,460,378)}
RESULT=[]

def sha(data):return hashlib.sha256(data).hexdigest()
def snap(root):
    return {str(x.relative_to(root)):(sha(x.read_bytes()),stat.S_IMODE(x.stat().st_mode)) for dir in ('app','system') for x in (root/dir).rglob('*') if x.is_file()}
def run(sd,env):
    return subprocess.run(['sh',str(sd/'mod/custom.sh')],env=env,capture_output=True,text=True,timeout=60)
def prep(upgrade):
    root=Path(tempfile.mkdtemp(prefix='v8r-installer-'))
    sd=root/'sd';shutil.copytree(SD,sd)
    app=root/'app';sys=root/'system';lsd=app/'eso/hmi/lsd'
    (app/'root/hooks').mkdir(parents=True)
    (lsd/'jars').mkdir(parents=True)
    (lsd/'DSITracer.jar').write_text('STOCK MOCK DSITracer')
    with zipfile.ZipFile(FW) as z:
        b='v8i-full-diagnostic/collected/run-01/firmware/files/'
        for prefix,name in [('8-','bundles_dsitracer_enabled.properties'),('11-','traceConfig.properties')]:
            (lsd/name).write_bytes(z.read(b+prefix+name))
        smart=sys/'etc/eso/production/smartphone_integrator.json'
        smart.parent.mkdir(parents=True)
        smart.write_bytes(z.read(b+'36-smartphone_integrator.json'))
    if upgrade:
        with zipfile.ZipFile(BASE) as z:
            b='mod/mib2high-carplay-rgi-v8p-simple/payload/'
            for name in ['libmib2high-carplay-rgi.so','mib2high-carplay-rgi.jar','vc_render','mib2high-lvds-control.sh','mib2high-lvds-profile.conf','mib2high-lvds-viewareas.conf','v8l-dsitracer-capture']:
                dest=app/'root/hooks'/name if name.endswith('.so') else (lsd/'jars'/name if name.endswith('.jar') else app/'root'/name)
                dest.write_bytes(z.read(b+name))
                dest.chmod(0o755 if name.endswith(('.so','.sh','capture')) or name=='vc_render' else 0o644)
            p=root/'oldtrace.awk';p.write_bytes(z.read('mod/mib2high-carplay-rgi-v8p-simple/source/trace.awk'))
            t=subprocess.run(['awk','-f',str(p)],input=(lsd/'traceConfig.properties').read_text(),capture_output=True,text=True,check=True)
            (lsd/'traceConfig.properties').write_text(t.stdout)
        (app/'carplay_altscreen').touch();(lsd/'dsitracer_activated').touch()
    fake=root/'fakebin';fake.mkdir()
    for cmd in ['sloginfo','pidin','netstat']:
        p=fake/cmd;p.write_text('#!/bin/sh\necho MOCK_'+cmd+'\n');p.chmod(0o755)
    shmem=root/'shmem';shmem.mkdir()
    env={**os.environ,'PATH':str(fake)+':'+os.environ['PATH'],'MIB_SD':str(sd),'MIB_APP':str(app),'MIB_SYSTEM':str(sys),'MIB_RCC_SHMEM':str(shmem)}
    return root,sd,app,sys,lsd,shmem,env

# Configuration and payload consistency
config=(S/'config/mib2high-lvds-viewareas.conf').read_text()
assert 'physical 200 75' in config
for view,xywh in EXPECT.items():
    assert 'view %d %s'%(view,' '.join(map(str,xywh))) in config
assert (SD/'mod'/PK/'payload/mib2high-lvds-viewareas.conf').read_text()==config
assert (SD/'mod'/PK/'payload/libmib2high-carplay-rgi.so').read_bytes()==(S/'output/libmib2high-carplay-rgi.so').read_bytes()
with zipfile.ZipFile(BASE) as z:
    b='mod/mib2high-carplay-rgi-v8p-simple/payload/'
    for name in ['mib2high-carplay-rgi.jar','vc_render','mib2high-lvds-control.sh','mib2high-lvds-profile.conf','v8l-dsitracer-capture']:
        assert (SD/'mod'/PK/'payload'/name).read_bytes()==z.read(b+name), name
assert 'ALTSCREEN_URL_BIG_GAUGES' in (S/'native/altscreen_airplay.c').read_text()
assert 'make_allowed_cluster_urls' in (S/'native/altscreen_airplay.c').read_text()
assert 'make_cluster_urls();' in (S/'native/altscreen_airplay.c').read_text()
assert 'altscreen_gauge_url_for_view(view)' in (S/'native/altscreen_session.c').read_text()
assert 'send_event(receiver, "showUI",' in (S/'native/altscreen_session.c').read_text()
assert 'send_event(state->receiver, "showUI", altscreen_gauge_url_for_view(view))' in (S/'native/altscreen_session.c').read_text()
assert (S/'native/altscreen.h').read_bytes()==(W/'v8p_source/v8p_source/native/altscreen.h').read_bytes() # initialURL unchanged
RESULT.append('Arctus FULL/SMALL geometry, view index mapping, query URL wiring, unchanged native initialURL and payload binary hashes: PASS')

# Full installer source is shell-valid, version names isolated, rollback first
script=(SD/'mod/custom.sh').read_text()
assert subprocess.run(['sh','-n',str(SD/'mod/custom.sh')]).returncode==0
assert 'v8p' not in script.lower() and 'v8r' in script.lower()
assert script.index('if [ -f "$ROLLBACK" ]')<script.index('elif [ -f "$INSTALLED" ]')
RESULT.append('shell syntax, versioned package paths, rollback-first dispatch: PASS')

# Clean install, log-collection pass, exact rollback
root,sd,app,sys,lsd,shmem,env=prep(False)
try:
    before=snap(root)
    p=run(sd,env);assert p.returncode==0 and 'INSTALL_RESULT=PASS' in p.stdout,(p.returncode,p.stdout,p.stderr)
    installed=(app/'root/hooks/libmib2high-carplay-rgi.so').read_bytes()
    assert installed==(S/'output/libmib2high-carplay-rgi.so').read_bytes()
    assert (app/'root/mib2high-lvds-viewareas.conf').read_text()==config
    dsi=Path('/tmp/v8l-dsitracer-1798-00.log');dsi.write_text('MOCK_DSIBAP54\n')
    (shmem/'v8m-rgi-factory.log').write_text('V8M_FACTORY MOCK\n')
    p=run(sd,env);assert p.returncode==0 and 'COLLECT_RESULT=PASS' in p.stdout,(p.returncode,p.stdout)
    collection=sd/'mod'/PK/'collected/run-01'
    assert (collection/'live/libmib2high-carplay-rgi.so').read_bytes()==installed
    assert (collection/'live/mib2high-lvds-viewareas.conf').read_text()==config
    assert 'COLLECTION_VERSION=v8r' in (collection/'trace/metadata.txt').read_text()
    (sd/'mod'/PK/'ROLLBACK').touch();p=run(sd,env)
    assert p.returncode==0 and 'ROLLBACK_RESULT=PASS' in p.stdout,(p.returncode,p.stdout)
    assert snap(root)==before
    RESULT.append('clean stock install + full collection + byte/mode-exact rollback: PASS')
finally:
    Path('/tmp/v8l-dsitracer-1798-00.log').unlink(missing_ok=True)
    shutil.rmtree(root)

# Existing v8P upgrade, exact rollback to v8P
root,sd,app,sys,lsd,shmem,env=prep(True)
try:
    before=snap(root)
    p=run(sd,env);assert p.returncode==0 and 'INSTALL_RESULT=PASS' in p.stdout,(p.returncode,p.stdout)
    assert (app/'root/mib2high-lvds-viewareas.conf').read_text()==config
    assert (app/'root/hooks/libmib2high-carplay-rgi.so').read_bytes()==(S/'output/libmib2high-carplay-rgi.so').read_bytes()
    (sd/'mod'/PK/'ROLLBACK').touch();p=run(sd,env)
    assert p.returncode==0 and 'ROLLBACK_RESULT=PASS' in p.stdout,(p.returncode,p.stdout)
    assert snap(root)==before
    RESULT.append('v8P→v8R upgrade and exact v8P rollback: PASS')
finally:shutil.rmtree(root)

# Forced mid-install error: modify only temporary mock installer copy, not release artifact
root,sd,app,sys,lsd,shmem,env=prep(True)
try:
    before=snap(root)
    inst=sd/'mod/custom.sh';script=inst.read_text()
    old='install_file "$PAY/$n" "$dest" `modeof "$n"` || fail "install failed $n"'
    assert old in script
    inst.write_text(script.replace(old,'if [ "$n" = mib2high-lvds-control.sh ]; then false; else install_file "$PAY/$n" "$dest" `modeof "$n"`; fi || fail "install failed $n"'))
    p=run(sd,env)
    assert p.returncode!=0 and 'AUTO_ROLLBACK_RESULT=PASS' in p.stdout,(p.returncode,p.stdout,p.stderr)
    assert snap(root)==before
    assert not (sd/'mod'/PK/'installed.ok').exists()
    RESULT.append('injected failure at third payload write: auto rollback to exact v8P state: PASS')
finally:shutil.rmtree(root)

# Ensure failed collection does not report success
root,sd,app,sys,lsd,shmem,env=prep(False)
try:
    p=run(sd,env);assert p.returncode==0 and 'INSTALL_RESULT=PASS' in p.stdout
    p=run(sd,env);assert p.returncode==0 and 'COLLECT_RESULT=PARTIAL' in p.stdout,p.stdout
    RESULT.append('missing logs correctly produces PARTIAL collection: PASS')
finally:shutil.rmtree(root)
print(json.dumps({'release':'v8R','offline_only':True,'results':RESULT},indent=2))
