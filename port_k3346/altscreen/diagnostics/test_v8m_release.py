#!/usr/bin/env python3
"""Offline source-config, self-contained installer, upgrade/rollback, and collector tests."""
import os, shutil, subprocess, tempfile, stat, json, zipfile, pathlib, hashlib
ROOT=pathlib.Path('/mnt/data/v8m_work'); SD=ROOT/'sd_runtime'; V8L=pathlib.Path('/mnt/data/K2161_V8L_RGI_VC_GAUGE_DIAGNOSTICS_INSTALL_COLLECT.zip'); FIRMWARE=pathlib.Path('/mnt/data/v8i-full-diagnostic-2026-10-10.zip')
PKG='mib2high-carplay-rgi-v8m-simple'; trace=ROOT/'v8m_source/deployment/trace.awk'
results=[]
def run_script(path,env):
 p=subprocess.run(['sh',str(path)],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
 return p.returncode,p.stdout
def snapshot(root):
 return {str(p.relative_to(root)):(hashlib.sha256(p.read_bytes()).hexdigest(),stat.S_IMODE(p.stat().st_mode)) for p in list((root/'app').rglob('*'))+list((root/'system').rglob('*')) if p.is_file()}
def setup(upgrade=False):
 root=pathlib.Path(tempfile.mkdtemp(prefix='v8m-test-')); sd=root/'sd';shutil.copytree(SD,sd)
 app=root/'app';sys=root/'system';lsd=app/'eso/hmi/lsd';lsd.mkdir(parents=True)
 (app/'root/hooks').mkdir(parents=True);(lsd/'jars').mkdir()
 (lsd/'DSITracer.jar').write_text('MOCK STOCK DSITracer')
 with zipfile.ZipFile(FIRMWARE) as z:
  base='v8i-full-diagnostic/collected/run-01/firmware/files/'
  for seq,name in [('8-','bundles_dsitracer_enabled.properties'),('11-','traceConfig.properties')]:
   (lsd/name).write_bytes(z.read(base+seq+name))
  smart=sys/'etc/eso/production/smartphone_integrator.json';smart.parent.mkdir(parents=True);smart.write_bytes(z.read(base+'36-smartphone_integrator.json'))
 if upgrade:
  with zipfile.ZipFile(V8L) as z:
   b='mod/mib2high-carplay-rgi-v8l-simple/payload/'
   for name in ['libmib2high-carplay-rgi.so','mib2high-carplay-rgi.jar','vc_render','mib2high-lvds-control.sh','mib2high-lvds-profile.conf','mib2high-lvds-viewareas.conf','v8l-dsitracer-capture']:
    if name.endswith('.so'): dest=app/'root/hooks'/name
    elif name.endswith('.jar'): dest=lsd/'jars'/name
    else:dest=app/'root'/name
    dest.write_bytes(z.read(b+name));dest.chmod(0o755 if name.endswith(('.so','.sh','capture')) or name=='vc_render' else 0o644)
  (app/'carplay_altscreen').touch();(lsd/'dsitracer_activated').touch()
  # Actual installed v8l DSI config as additional realistic pre-v8m input.
  baseline=(lsd/'traceConfig.properties').read_text()
  old_awk=ROOT/'v8l_source_trace.awk'
  with zipfile.ZipFile(V8L) as z:old_awk.write_bytes(z.read('mod/mib2high-carplay-rgi-v8l-simple/source/trace.awk'))
  cp=subprocess.run(['awk','-f',str(old_awk)],input=baseline,text=True,capture_output=True,check=True)
  (lsd/'traceConfig.properties').write_text(cp.stdout)
 stub=root/'fakebin';stub.mkdir()
 for cmd in ['sloginfo','pidin','netstat']:
  p=stub/cmd;p.write_text('#!/bin/sh\necho MOCK_'+cmd+'\n');p.chmod(0o755)
 shmem=root/'rcc';shmem.mkdir()
 env={**os.environ,'MIB_SD':str(sd),'MIB_APP':str(app),'MIB_SYSTEM':str(sys),'MIB_RCC_SHMEM':str(shmem),'PATH':str(stub)+':'+os.environ['PATH']}
 return root,sd,app,sys,lsd,shmem,env
# Validate AWK patch against v8l's real config and malformed variants.
with zipfile.ZipFile(FIRMWARE) as z:
 b='v8i-full-diagnostic/collected/run-01/firmware/files/11-traceConfig.properties'
 original=z.read(b).decode('utf-8')
for text_in in [original, 'custom.unrelated=survive\n',original+'\norg.dsi.ifc.bap.DSIBAP=small\n#org.dsi.ifc.bap.DSIBAP=full\norg.dsi.ifc.carkombi.DSICarKombi=off\n']:
 p=subprocess.run(['awk','-f',str(trace)],input=text_in,text=True,capture_output=True,check=True); t=p.stdout
 for key in ['org.dsi.ifc.bap.DSIBAP=full','org.dsi.ifc.carkombi.DSICarKombi=full','org.dsi.ifc.komoview.DSIKOMOView=full','org.dsi.ifc.keypanel.DSIKeyPanel=full']:
  assert t.splitlines().count(key)==1,(key,t)
 assert 'custom.unrelated=survive' not in text_in or 'custom.unrelated=survive' in t
 assert 'org.dsi.ifc.bap.DSIBAP=small' not in t and 'org.dsi.ifc.carkombi.DSICarKombi=off' not in t
results.append('source AWK enables correct seven DSI filters with duplicate/missing/alternative values: PASS')
# Realistic v8l->v8m upgrade and exact rollback, plus collector
root,sd,app,sys,lsd,shmem,env=setup(True)
try:
 original_state=snapshot(root)
 rc,out=run_script(sd/'mod/custom.sh',env);assert rc==0 and 'INSTALL_RESULT=PASS' in out,out
 c=(lsd/'traceConfig.properties').read_text()
 assert c.splitlines().count('org.dsi.ifc.bap.DSIBAP=full')==1
 assert c.splitlines().count('org.dsi.ifc.carkombi.DSICarKombi=full')==1
 assert (lsd/'jars/mib2high-carplay-rgi.jar').read_bytes()==(ROOT/'v8m_source/output/mib2high-carplay-rgi.jar').read_bytes()
 assert hashlib.sha256((app/'root/v8l-dsitracer-capture').read_bytes()).hexdigest()==original_state['app/root/v8l-dsitracer-capture'][0]
 assert (sd/'mod'/PKG/'backup/pre-v8m/complete').exists()
 dsi=pathlib.Path('/tmp/v8l-dsitracer-1798-00.log');dsi.write_text('MOCK DSI BAP fctID=23 and 54, VIEW key=35\n')
 factory=shmem/'v8m-rgi-factory.log';factory.write_text('V8M_FACTORY SNAPSHOT factoryLargeMapView=false actualViewSize=1\n')
 rc,out=run_script(sd/'mod/custom.sh',env);assert rc==0 and 'COLLECT_RESULT=PASS' in out,out
 logs=sd/'mod'/PKG/'collected/run-01/logs'
 assert (logs/factory.name).read_bytes()==factory.read_bytes()
 assert (logs/dsi.name).read_bytes()==dsi.read_bytes()
 assert 'COLLECTION_VERSION=v8m' in (sd/'mod'/PKG/'collected/run-01/trace/metadata.txt').read_text()
 (sd/'mod'/PKG/'ROLLBACK').touch();rc,out=run_script(sd/'mod/custom.sh',env)
 assert rc==0 and 'ROLLBACK_RESULT=PASS' in out,out
 assert snapshot(root)==original_state,[(k,original_state.get(k),snapshot(root).get(k)) for k in sorted(set(original_state)|set(snapshot(root))) if original_state.get(k)!=snapshot(root).get(k)]
 results.append('v8l installed -> v8m source-built JAR/config -> collect -> exact v8l bytes+modes rollback: PASS')
finally:
 pathlib.Path('/tmp/v8l-dsitracer-1798-00.log').unlink(missing_ok=True);shutil.rmtree(root)
# Fresh install clean and rollback, no project-owned files preinstalled
root,sd,app,sys,lsd,shmem,env=setup(False)
try:
 original_state=snapshot(root)
 rc,out=run_script(sd/'mod/custom.sh',env);assert rc==0 and 'INSTALL_RESULT=PASS' in out,out
 assert (app/'root/hooks/libmib2high-carplay-rgi.so').exists() and (app/'root/vc_render').exists()
 assert (app/'root/v8l-dsitracer-capture').exists() and (app/'root/mib2high-lvds-control.sh').exists()
 assert (app/'carplay_altscreen').exists()
 (sd/'mod'/PKG/'ROLLBACK').touch();rc,out=run_script(sd/'mod/custom.sh',env)
 assert rc==0 and 'ROLLBACK_RESULT=PASS' in out,out
 assert snapshot(root)==original_state
 results.append('fresh stock install (no project artifacts) and exact absent-file rollback: PASS')
finally:shutil.rmtree(root)
# Interruption after some files have been installed: auto-rollback byte-perfect.
root,sd,app,sys,lsd,shmem,env=setup(True)
try:
 original_state=snapshot(root)
 p=sd/'mod/custom.sh';t=p.read_text()
 needle='install_file "$PAY/$n" "$dest" `modeof "$n"` || fail "install failed $n"'
 assert needle in t
 t=t.replace(needle,'if [ "$n" = mib2high-lvds-control.sh ]; then false; else install_file "$PAY/$n" "$dest" `modeof "$n"`; fi || fail "install failed $n"')
 p.write_text(t)
 rc,out=run_script(p,env)
 assert rc!=0 and 'AUTO_ROLLBACK_RESULT=PASS' in out,out
 assert snapshot(root)==original_state
 results.append('forced mid-install failure restores exact pre-v8m bytes/modes: PASS')
finally:shutil.rmtree(root)
print(json.dumps({'offline_only':True,'results':results},indent=2))
