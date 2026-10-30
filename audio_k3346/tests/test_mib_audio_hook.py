"""Host filesystem simulation only; never executes on a vehicle."""
import hashlib,os,shutil,subprocess,tempfile,unittest,re,shlex
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PACKAGE=Path(os.environ.get('MHI2_AUDIO_TEST_PACKAGE',str(ROOT/'artifacts/mib-audio-hook-draft-v7')))
RETURN=ROOT/'handoff-windows/MHI2_RETURN_20261015-20261015-073857'

LIBS=['libmhi2_audio_hook.so','libmhi2_apac.so','libmhi2_aac.so','libmhi2_fdk.so']

class InstallerTest(unittest.TestCase):
 def setUp(self):
  work=ROOT/'artifacts/installer-test-work';work.mkdir(exist_ok=True)
  self.root=Path(tempfile.mkdtemp(prefix='mhi2-audio-install-',dir=str(work)))
  self.sd=self.root/'sd';shutil.copytree(PACKAGE,self.sd)
  self.p=self.sd/'mhi2-audio';self.app=self.root/'app';self.system=self.root/'system';self.bin=self.root/'bin';self.bin.mkdir()
  self.hmi=self.root/'lsd.jxe';os.link(RETURN/'evidence/localization/binaries/lsd.jxe',self.hmi)
  mapping={'eso/lib/libairplay.so':'libairplay.so','armle/usr/lib/libnvaudio.so':'libnvaudio.so','root/bin-target/deva-nvaudio':'deva-nvaudio','eso/bin/apps/smartphone_integrator':'smartphone_integrator','eso/bin/apps/mediarouter':'mediarouter','eso/bin/apps/dio_manager':'dio_manager'}
  for dest,name in mapping.items():
   q=self.app/dest;q.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(RETURN/'evidence/binaries'/name,q)
  self.lsd=self.app/'eso/hmi/lsd';(self.lsd/'jars').mkdir(parents=True)
  shutil.copy2(self.p/'stock/lsd.sh',self.lsd/'lsd.sh');shutil.copy2(self.p/'stock/lang-stock.zip',self.lsd/'jars/lang_data.zip')
  self.config=self.system/'etc/eso/production/smartphone_integrator.json';self.config.parent.mkdir(parents=True);shutil.copy2(self.p/'stock/smartphone_integrator.json',self.config)
  self.orig={q:q.read_bytes() for q in [self.lsd/'lsd.sh',self.lsd/'jars/lang_data.zip',self.config]}
  helpers={'uname':'echo QNX','df':'echo "Filesystem Size Used Avail Capacity Mounted"; echo "test 100000 100 99900 1% test"','mount':'exit 0','sync':'exit 0','pidin':'echo "1234 smartphone_integrator"'}
  for name,body in helpers.items():self.executable(self.bin/name,'#!/bin/sh\n'+body+'\n')
  self.executable(self.bin/'sha1sum','#!/usr/bin/env python3\nimport sys,hashlib\nfrom pathlib import Path\np=Path(sys.argv[1]);print(hashlib.sha1(p.read_bytes()).hexdigest()+"  "+str(p))\n')
  self.executable(self.bin/'awk','#!/bin/sh\nexec /usr/bin/awk "$@"\n')
  script=self.p/'scripts/audio.sh';s=script.read_text()
  s=re.sub(r'^PATH=.*$', 'PATH='+shlex.quote(str(self.bin)+':'+os.environ['PATH']),s,flags=re.M)
  for key,value in [('APP',self.app),('SYSTEM',self.system),('SHA',self.bin/'sha1sum'),('AWK',self.bin/'awk'),('LOCK',self.root/'lock')]:
   s=re.sub(r'^'+key+r'=.*$',key+'='+shlex.quote(str(value)),s,flags=re.M)
  s=s.replace('file_sha1 /ifs/lsd.jxe','file_sha1 '+shlex.quote(str(self.hmi)))
  script.write_text(s)
  lines=(self.p/'SHA1SUMS').read_text().splitlines();lines=[hashlib.sha1(script.read_bytes()).hexdigest()+'  scripts/audio.sh' if line.endswith('  scripts/audio.sh') else line for line in lines];(self.p/'SHA1SUMS').write_text('\n'.join(lines)+'\n')
 def tearDown(self):
  shutil.rmtree(self.root)
 def executable(self,p,s):p.write_text(s);p.chmod(0o755)
 def run_action(self,action):
  (self.p/'ACTION.txt').write_text(action+'\n')
  return subprocess.run([os.environ.get('MHI2_AUDIO_TEST_SHELL','/bin/sh'),str(self.p/'scripts/audio.sh')],text=True,capture_output=True,timeout=40)
 def unchanged(self):
  for q,b in self.orig.items():self.assertEqual(q.read_bytes(),b)
 def test_check_no_app_changes(self):
  result=self.run_action('CHECK');self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.unchanged();self.assertFalse((self.app/'root/mhi2-audio').exists())
 def test_install_restore_stock(self):
  result=self.run_action('INSTALL_PREVIEW');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
  self.assertEqual((self.p/'ACTION.txt').read_text(),'CHECK\n');self.assertTrue((self.app/'root/mhi2-audio/FIRST_BOOT').is_file())
  self.assertIn('libmhi2_audio_hook.so',self.config.read_text());self.assertIn('AUDIO_SPATIAL_CODEC_LIB',self.config.read_text())
  for n in LIBS:self.assertEqual((self.app/'eso/lib'/n).read_bytes(),(self.p/'payload'/n).read_bytes());self.assertEqual((self.lsd/'jars/lang_data.zip').read_bytes(),(self.p/'payload/lang-stock.zip').read_bytes())
  result=self.run_action('CHECK');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
  result=self.run_action('RESTORE');self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.unchanged()
  [self.assertFalse((self.app/'eso/lib'/n).exists()) for n in LIBS];self.assertFalse((self.lsd/'jars/mhi2_audio_ui.jar').exists())
  result=self.run_action('CHECK');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
 def test_tamper_rejected_before_mutation(self):
  with (self.p/'payload/libmhi2_apac.so').open('ab') as f:f.write(b'bad')
  result=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(result.returncode,0);self.assertIn('Package hash mismatch',result.stdout);self.unchanged();self.assertFalse((self.app/'root/mhi2-audio').exists());self.assertEqual((self.p/'ACTION.txt').read_text(),'CHECK\n')
 def test_core_mismatch_rejected(self):
  (self.app/'eso/lib/libairplay.so').write_bytes(b'wrong train')
  result=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(result.returncode,0);self.assertIn('Unsupported core binary',result.stdout);self.unchanged();self.assertFalse((self.app/'root/mhi2-audio').exists())
 def test_foreign_language_module_rejected(self):
  target=self.lsd/'jars/lang_data.zip';target.write_bytes(b'foreign localization module');self.orig[target]=target.read_bytes()
  result=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(result.returncode,0);self.assertIn('standalone audio stock baseline',result.stdout);self.unchanged();self.assertFalse((self.app/'root/mhi2-audio').exists())
 def test_connected_phone_rejected(self):
  self.executable(self.bin/'pidin','#!/bin/sh\necho "1234 /mnt/app/eso/bin/apps/dio_manager"\n')
  result=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(result.returncode,0);self.assertIn('Disconnect iPhone',result.stdout);self.unchanged();self.assertFalse((self.app/'root/mhi2-audio').exists())
 def test_installer_uses_no_tool_missing_on_unit(self):
  # Harvest 2026-10-22: the unit has no printf, tr, sed, date, du, which or sha*; only M.I.B. sha1sum/awk via $SHA/$AWK.
  # A missing printf in the CarPlay-connected guard made the awk pipeline return "not found" and the guard fail open.
  text=(self.p/'scripts/audio.sh').read_text()
  for tool in ('printf','tr','sed','date','du','which','basename','cmp','mktemp','timeout'):
   self.assertIsNone(re.search(r'(^|[;&|(`!]|\$\(|\b(?:if|then|do|else|while|until))\s*'+tool+r'\s',text,flags=re.M),tool+' is not on the unit')
 def test_partial_install_can_restore(self):
  body='#!/bin/sh\ncase "$2" in *smartphone_integrator.json.audio-new) exit 1 ;; esac\nexec /bin/mv "$@"\n'
  # Fail the last activation after native/UI/language/startup already changed.
  self.executable(self.bin/'mv',body.replace('case "$2"','case "$1"'))
  result=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(result.returncode,0)
  self.assertTrue((self.app/'root/mhi2-audio/baseline/COMPLETE').exists())
  self.executable(self.bin/'mv','#!/bin/sh\nexec /bin/mv "$@"\n')
  result=self.run_action('RESTORE');self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.unchanged()
 def test_foreign_library_blocks_install(self):
  q=self.app/'eso/lib/libmhi2_fdk.so';q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(b'foreign')
  result=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(result.returncode,0);self.assertIn('Audio files already exist',result.stdout);self.unchanged();self.assertFalse((self.app/'root/mhi2-audio').exists())
 def test_package_is_inert_build(self):
  import json;meta=json.loads((self.p/'package.json').read_text());self.assertEqual(meta['hook_variant'],'MHI2_NV_SIX_CHANNEL_QUALIFIED=0');self.assertFalse(meta['advertises_5_1']);self.assertFalse(meta['claims_stock_audio'])
  self.assertEqual((self.p/'payload/libmhi2_audio_hook.so').read_bytes(),(ROOT/'artifacts/native-hook-so-inert/libmhi2_audio_hook.so').read_bytes())
 def test_failure_at_every_activation_can_restore(self):
  # Fail the atomic move of each owned file in turn; the sealed baseline must always allow a clean RESTORE.
  targets=['libmhi2_audio_hook.so','libmhi2_apac.so','libmhi2_aac.so','libmhi2_fdk.so','mhi2_audio_ui.jar','lang_data.zip','smartphone_integrator.json']
  for name in targets:
   with self.subTest(failing=name):
    self.executable(self.bin/'mv','#!/bin/sh\ncase "$1" in *'+name+'.audio-new) exit 1 ;; esac\nexec /bin/mv "$@"\n')
    result=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(result.returncode,0,name)
    self.assertTrue((self.app/'root/mhi2-audio/baseline/COMPLETE').exists())
    self.executable(self.bin/'mv','#!/bin/sh\nexec /bin/mv "$@"\n')
    result=self.run_action('RESTORE');self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.unchanged()
    [self.assertFalse((self.app/'eso/lib'/n).exists()) for n in LIBS];self.assertFalse((self.lsd/'jars/mhi2_audio_ui.jar').exists())
    shutil.rmtree(self.app/'root/mhi2-audio')
 def test_reinstall_after_restore_requires_archive(self):
  self.assertEqual(self.run_action('INSTALL_PREVIEW').returncode,0);self.assertEqual(self.run_action('RESTORE').returncode,0)
  result=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(result.returncode,0);self.assertIn('Restored backup preserved',result.stdout);self.unchanged()
 def test_low_space_rejected(self):
  self.executable(self.bin/'df','#!/bin/sh\necho "Filesystem Size Used Avail Capacity Mounted"; echo "test 100000 99000 1000 99% test"\n')
  result=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(result.returncode,0);self.assertIn('free required',result.stdout);self.unchanged();self.assertFalse((self.app/'root/mhi2-audio').exists())
 def foreign_preload(self):
  text=self.config.read_text()
  text=text.replace('"IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production"','"IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production",\n                "LD_PRELOAD=/mnt/app/root/hooks/libmib2high-carplay-rgi.so"',1)
  self.config.write_text(text);self.orig[self.config]=self.config.read_bytes()
 def test_lsd_sh_is_never_touched(self):
  self.assertFalse((self.p/'payload/lsd.sh').exists())
  self.assertEqual(self.run_action('INSTALL_PREVIEW').returncode,0);self.assertEqual((self.lsd/'lsd.sh').read_bytes(),self.orig[self.lsd/'lsd.sh'])
  self.assertEqual(self.run_action('RESTORE').returncode,0);self.unchanged()
 def test_lsd_sh_modified_rejected(self):
  q=self.lsd/'lsd.sh';q.write_bytes(q.read_bytes()+b'# other change\n');self.orig[q]=q.read_bytes()
  result=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(result.returncode,0);self.assertIn('Startup differs',result.stdout);self.unchanged();self.assertFalse((self.app/'root/mhi2-audio').exists())
 def test_coexists_with_foreign_preload_and_restores_exactly(self):
  self.foreign_preload();before=self.config.read_text().count('LD_PRELOAD=')
  result=self.run_action('INSTALL_PREVIEW');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
  text=self.config.read_text();self.assertIn('"LD_PRELOAD=/mnt/app/root/hooks/libmib2high-carplay-rgi.so:/mnt/app/eso/lib/libmhi2_audio_hook.so"',text);self.assertEqual(text.count('LD_PRELOAD='),before)
  self.assertEqual(self.run_action('CHECK').returncode,0)
  result=self.run_action('RESTORE');self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.unchanged()
 def test_restore_keeps_entries_added_after_install(self):
  self.assertEqual(self.run_action('INSTALL_PREVIEW').returncode,0)
  text=self.config.read_text().replace('"IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production"','"IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production",\n                "OTHER_COMPONENT=1"',1);self.config.write_text(text)
  result=self.run_action('RESTORE');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
  after=self.config.read_text();self.assertIn('OTHER_COMPONENT=1',after);self.assertNotIn('libmhi2_audio_hook',after);self.assertNotIn('AUDIO_SPATIAL_CODEC_LIB',after);self.assertIn('differs from the install-time baseline',result.stdout)
 def test_foreign_preload_replaced_by_other_installer_is_detected(self):
  self.assertEqual(self.run_action('INSTALL_PREVIEW').returncode,0)
  text='\n'.join(l for l in self.config.read_text().split('\n') if 'libmhi2_audio_hook' not in l);self.config.write_text(text)
  result=self.run_action('CHECK');self.assertNotEqual(result.returncode,0);self.assertIn('Installed child config changed',result.stdout)
  self.assertEqual(self.run_action('RESTORE').returncode,0)
 def test_unsupported_config_layout_rejected(self):
  self.config.write_text(self.config.read_text().replace('"carplay":{','"carplay_x":{'));self.orig[self.config]=self.config.read_bytes()
  result=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(result.returncode,0);self.assertIn('Child config is not ready',result.stdout);self.unchanged();self.assertFalse((self.app/'root/mhi2-audio').exists())
 def test_installer_avoids_unit_pitfalls(self):
  text=(self.p/'scripts/audio.sh').read_text()
  for alias in ('hash','type','r','stop','history','integer','local','nohup','login','autoload','functions','suspend'):
   self.assertIsNone(re.search(r'^\s*'+alias+r'\s*\(\)',text,flags=re.M),alias+' is a ksh alias on the unit')
  self.assertNotIn('mkdir "$LOCK"',text);self.assertNotIn(': >',text);self.assertNotIn('tail -1',text)
  entry=(self.sd/'mod/command.sh');self.assertTrue(entry.is_file())
 def test_installs_after_ukrainian_package_and_restores_exactly(self):
  target=self.lsd/'jars/lang_data.zip';shutil.copy2(self.p/'stock/lang-uk.zip',target);self.orig[target]=target.read_bytes()
  result=self.run_action('INSTALL_PREVIEW');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
  self.assertEqual(target.read_bytes(),(self.p/'payload/lang-uk.zip').read_bytes());self.assertNotEqual(target.read_bytes(),self.orig[target])
  import zipfile;z=zipfile.ZipFile(target);self.assertIn('resources/HMIToneEvoHigh/strings_uk_UA.data',z.namelist())
  self.assertEqual(self.run_action('CHECK').returncode,0)
  result=self.run_action('RESTORE');self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.unchanged()
if __name__=='__main__':unittest.main()
