#!/usr/bin/env python3
"""v8S offline packaging and recovery tests; never run on an MMI."""
from pathlib import Path
import tempfile, hashlib, shutil, zipfile, subprocess, os, stat, json
W=Path('/mnt/data/v8s_work')
S=W/'s'
PK='mib2high-carplay-rgi-v8s-simple'
SD=W/'sd'
BASE=Path('/mnt/data/K2161_V8R_ARCTUS_SAFEAREA_GAUGE_ALIGN_INSTALL_COLLECT_2.zip')
FW=Path('/mnt/data/v8i-full-diagnostic-2026-10-10.zip')
EXPECT={0:(360,65,720,367),1:(132,85,840,332),2:(490,72,460,378),3:(490,72,460,378)}
RESULT=[]

def sha(data):return hashlib.sha256(data).hexdigest()
def snap(root):
    return {str(x.relative_to(root)):(sha(x.read_bytes()),stat.S_IMODE(x.stat().st_mode)) for dir in ('app','system') for x in (root/dir).rglob('*') if x.is_file()}
def run(sd,env):
    return subprocess.run(['sh',str(sd/'mod/custom.sh')],env=env,capture_output=True,text=True,timeout=60)
def prep(upgrade):
    root=Path(tempfile.mkdtemp(prefix='v8s-installer-'))
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
            b='mod/mib2high-carplay-rgi-v8r-simple/payload/'
            for name in ['libmib2high-carplay-rgi.so','mib2high-carplay-rgi.jar','vc_render','mib2high-lvds-control.sh','mib2high-lvds-profile.conf','mib2high-lvds-viewareas.conf','v8l-dsitracer-capture']:
                dest=app/'root/hooks'/name if name.endswith('.so') else (lsd/'jars'/name if name.endswith('.jar') else app/'root'/name)
                dest.write_bytes(z.read(b+name))
                dest.chmod(0o755 if name.endswith(('.so','.sh','capture')) or name=='vc_render' else 0o644)
            p=root/'oldtrace.awk';p.write_bytes(z.read('mod/mib2high-carplay-rgi-v8r-simple/source/trace.awk'))
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
    b='mod/mib2high-carplay-rgi-v8r-simple/payload/'
    for name in ['mib2high-carplay-rgi.jar','vc_render','mib2high-lvds-control.sh','mib2high-lvds-profile.conf','v8l-dsitracer-capture']:
        assert (SD/'mod'/PK/'payload'/name).read_bytes()==z.read(b+name), name
assert 'ALTSCREEN_URL_BIG_GAUGES' in (S/'native/altscreen_airplay.c').read_text()
assert 'make_allowed_cluster_urls' in (S/'native/altscreen_airplay.c').read_text()
assert 'make_cluster_urls();' in (S/'native/altscreen_airplay.c').read_text()
src=(S/'native/altscreen_session.c').read_text()
assert 'v8s_control_completion' in src
assert 'v8s_control_tick' in src
assert 'ALTSCREEN_CONTROL_ACK_TIMEOUT_MS' in src
assert 'v8s_control_dispatch_failed' in src
assert 'state->last_ui_slot != desired / 2' in src
assert 'state->control_generation == ctx->generation' in src
assert 'rc = g_api.send_command(receiver, request, v8s_control_completion, ctx);' in src
assert (S/'native/altscreen.h').read_bytes()==(W/'r/native/altscreen.h').read_bytes()
assert (S/'native/gauge_layout_urls.h').read_text().count('leftAligned')==2
assert '\nview 1 132 85 840 332\n' in '\n'+(S/'config/mib2high-lvds-viewareas.conf').read_text()
RESULT.append('v8N small gauge route safe area, v8R big gauge geometry, completion-gated state machine, unchanged URL/interface: PASS')

# Full installer source is shell-valid, version names isolated, rollback first
script=(SD/'mod/custom.sh').read_text()
assert subprocess.run(['sh','-n',str(SD/'mod/custom.sh')]).returncode==0
assert 'v8r' not in script.lower() and 'v8s' in script.lower()
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
    assert 'COLLECTION_VERSION=v8s' in (collection/'trace/metadata.txt').read_text()
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
    RESULT.append('v8R→v8S upgrade and exact v8R rollback: PASS')
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
    RESULT.append('injected failure at third payload write: auto rollback to exact v8R state: PASS')
finally:shutil.rmtree(root)

# Ensure failed collection does not report success
root,sd,app,sys,lsd,shmem,env=prep(False)
try:
    p=run(sd,env);assert p.returncode==0 and 'INSTALL_RESULT=PASS' in p.stdout
    p=run(sd,env);assert p.returncode==0 and 'COLLECT_RESULT=PARTIAL' in p.stdout,p.stdout
    RESULT.append('missing logs correctly produces PARTIAL collection: PASS')
finally:shutil.rmtree(root)
print(json.dumps({'release':'v8S','offline_only':True,'results':RESULT},indent=2))
