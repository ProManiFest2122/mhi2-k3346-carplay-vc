#!/usr/bin/env python3
"""Simulate v8k already installed, upgrade v8l, rollback exact v8k bytes/modes."""
import zipfile,tempfile,shutil,subprocess,os,pathlib,hashlib,stat,json
ROOT=pathlib.Path(os.environ.get('V8L_SD_ROOT','/mnt/data/v8l_work/base_install'));V8K=pathlib.Path('/mnt/data/K2161_V8K_DIAGNOSTICS_INSTALL_COLLECT_FIXED.zip')
PKG='mib2high-carplay-rgi-v8l-simple'
def snapshot(root):
 return {str(p.relative_to(root)):(p.read_bytes(),stat.S_IMODE(p.stat().st_mode)) for p in root.rglob('*') if p.is_file() and not str(p).startswith(str(root/'sd'))}
root=pathlib.Path(tempfile.mkdtemp(prefix='v8l-upgrade-'))
try:
 sd=root/'sd';shutil.copytree(ROOT,sd)
 app=root/'app';system=root/'system';lsd=app/'eso/hmi/lsd';lsd.mkdir(parents=True)
 (app/'root/hooks').mkdir(parents=True);(lsd/'jars').mkdir(parents=True)
 (lsd/'DSITracer.jar').write_text('STOCK')
 from zipfile import ZipFile
 with ZipFile('/mnt/data/altscreen_inputs/v8i-full-diagnostic-2026-10-10.zip') as z:
  base='v8i-full-diagnostic/collected/run-01/firmware/files/'
  for seq,n in [('8-','bundles_dsitracer_enabled.properties'),('11-','traceConfig.properties')]:
   (lsd/n).write_bytes(z.read(base+seq+n))
  cfg=system/'etc/eso/production/smartphone_integrator.json';cfg.parent.mkdir(parents=True)
  cfg.write_bytes(z.read(base+'36-smartphone_integrator.json'))
 with ZipFile(V8K) as z:
  prefix='mod/mib2high-carplay-rgi-v8k-simple/payload/'
  payload=['libmib2high-carplay-rgi.so','mib2high-carplay-rgi.jar','vc_render','mib2high-lvds-control.sh','mib2high-lvds-profile.conf','mib2high-lvds-viewareas.conf','v8k-dsitracer-capture']
  for name in payload:
   if name.endswith('.so'): dest=app/'root/hooks'/name
   elif name.endswith('.jar'):dest=lsd/'jars'/name
   else:dest=app/'root'/name
   dest.write_bytes(z.read(prefix+name));dest.chmod(0o755 if name.endswith(('.so','.sh','capture')) or name=='vc_render' else 0o644)
 (app/'root/carplay_altscreen').touch()
 (lsd/'dsitracer_activated').touch()
 before=snapshot(root)
 fakebin=root/'fakebin';fakebin.mkdir()
 for c in ['sloginfo','pidin','netstat']:
  f=fakebin/c;f.write_text('#!/bin/sh\necho MOCK\n');f.chmod(0o755)
 env={**os.environ,'MIB_SD':str(sd),'MIB_APP':str(app),'MIB_SYSTEM':str(system),'PATH':str(fakebin)+':'+os.environ['PATH']}
 def run():
  p=subprocess.run(['sh',str(sd/'mod/custom.sh')],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=30)
  return p.returncode,p.stdout
 rc,out=run();assert rc==0 and 'INSTALL_RESULT=PASS' in out,out
 assert (sd/'mod'/PKG/'backup/pre-v8l/mib2high-carplay-rgi.jar').read_bytes()==before['app/eso/hmi/lsd/jars/mib2high-carplay-rgi.jar'][0]
 assert (app/'root/hooks/libmib2high-carplay-rgi.so').read_bytes()==before['app/root/hooks/libmib2high-carplay-rgi.so'][0]
 assert (app/'root/vc_render').read_bytes()==before['app/root/vc_render'][0]
 assert (lsd/'jars/mib2high-carplay-rgi.jar').read_bytes()!=before['app/eso/hmi/lsd/jars/mib2high-carplay-rgi.jar'][0]
 (pathlib.Path('/tmp/v8l-rgi-factory.log')).write_text('MOCK FACTORY TRACE\n')
 (pathlib.Path('/tmp/v8l-dsitracer-1798-00.log')).write_text('MOCK GAUGE TRACE\n')
 rc,out=run();assert rc==0 and 'COLLECT_RESULT=PASS' in out,out
 logs=sd/'mod'/PKG/'collected/run-01/logs'
 for n in ['v8l-rgi-factory.log','v8l-dsitracer-1798-00.log']:assert (logs/n).is_file(),n
 (sd/'mod'/PKG/'ROLLBACK').touch()
 rc,out=run();assert rc==0 and 'ROLLBACK_RESULT=PASS' in out,out
 after=snapshot(root)
 # account for mock temp fakes (added after snapshot) and SD is excluded
 after={k:v for k,v in after.items() if not k.startswith('fakebin/')}
 assert after==before,sorted(k for k in set(before)|set(after) if before.get(k)!=after.get(k))
 print(json.dumps({'v8k_installed_to_v8l_install':'PASS','collect':'PASS','rollback_exact_v8k_original_bytes_and_permissions':'PASS'},indent=2))
finally:
 for f in ['v8l-rgi-factory.log','v8l-dsitracer-1798-00.log']:(pathlib.Path('/tmp')/f).unlink(missing_ok=True)
 shutil.rmtree(root)
