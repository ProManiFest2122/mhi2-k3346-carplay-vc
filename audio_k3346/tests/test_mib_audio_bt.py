"""Host filesystem simulation of the BT/USB/SD package installer only; never executes on a vehicle."""
import hashlib,os,re,shlex,shutil,subprocess,tempfile,unittest,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PACKAGE=Path(os.environ.get('MHI2_AUDIO_BT_TEST_PACKAGE',str(ROOT/'artifacts/mib-audio-bt-draft-v5')))
RETURN=ROOT/'handoff-windows/MHI2_RETURN_20261015-20261015-073857'
LIBS=['libmhi2_audio_hook.so','libmhi2_apac.so','libmhi2_aac.so','libmhi2_fdk.so']

class BtInstallerTest(unittest.TestCase):
 def setUp(self):
  work=ROOT/'artifacts/installer-test-work';work.mkdir(exist_ok=True)
  self.root=Path(tempfile.mkdtemp(prefix='mhi2-bt-install-',dir=str(work)))
  self.sd=self.root/'sd';shutil.copytree(PACKAGE,self.sd)
  self.p=self.sd/'mhi2-audio-bt';self.app=self.root/'app';self.bin=self.root/'bin';self.bin.mkdir()
  self.hmi=self.root/'lsd.jxe';os.link(RETURN/'evidence/localization/binaries/lsd.jxe',self.hmi)
  for dest,name in {'armle/usr/lib/libnvaudio.so':'libnvaudio.so','root/bin-target/deva-nvaudio':'deva-nvaudio'}.items():
   q=self.app/dest;q.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(RETURN/'evidence/binaries'/name,q)
  self.mm=self.app/'armle/usr/sbin/mm-renderer';self.mm.parent.mkdir(parents=True);shutil.copy2(self.p/'stock/mm-renderer',self.mm)
  (self.app/'eso/lib').mkdir(parents=True);(self.app/'root').mkdir(exist_ok=True)
  self.libdir=self.app/'eso/lib/mhi2bt';self.real=self.libdir/'real/mm-renderer'
  self.orig=self.mm.read_bytes()
  helpers={'uname':'echo QNX','df':'echo "Filesystem Size Used Avail Capacity Mounted"; echo "test 100000 100 99900 1% test"','mount':'exit 0','sync':'exit 0'}
  for n,b in helpers.items():self.executable(self.bin/n,'#!/bin/sh\n'+b+'\n')
  self.executable(self.bin/'sha1sum','#!/usr/bin/env python3\nimport sys,hashlib\nfrom pathlib import Path\np=Path(sys.argv[1]);print(hashlib.sha1(p.read_bytes()).hexdigest()+"  "+str(p))\n')
  script=self.p/'scripts/bt.sh';s=script.read_text()
  s=re.sub(r'^PATH=.*$','PATH='+shlex.quote(str(self.bin)+':'+os.environ['PATH']),s,flags=re.M)
  for key,value in [('APP',self.app),('SHA',self.bin/'sha1sum'),('LOCK',self.root/'lock')]:
   s=re.sub(r'^'+key+r'=.*$',key+'='+shlex.quote(str(value)),s,flags=re.M)
  s=s.replace('hash /ifs/lsd.jxe','hash '+shlex.quote(str(self.hmi)))
  script.write_text(s)
  lines=(self.p/'SHA1SUMS').read_text().splitlines()
  lines=[hashlib.sha1(script.read_bytes()).hexdigest()+'  scripts/bt.sh' if l.endswith('  scripts/bt.sh') else l for l in lines];(self.p/'SHA1SUMS').write_text('\n'.join(lines)+'\n')
 def tearDown(self):shutil.rmtree(self.root)
 def executable(self,p,s):p.write_text(s);p.chmod(0o755)
 def run_action(self,a):
  (self.p/'ACTION.txt').write_text(a+'\n');return subprocess.run(['/bin/sh',str(self.p/'scripts/bt.sh')],text=True,capture_output=True,timeout=40)
 def ok(self,a):
  r=self.run_action(a);self.assertEqual(r.returncode,0,r.stdout+r.stderr);return r
 def bad(self,a,text):
  r=self.run_action(a);self.assertNotEqual(r.returncode,0);self.assertIn(text,r.stdout);self.assertEqual(self.mm.read_bytes(),self.orig);self.assertFalse((self.app/'root/mhi2-audio-bt').exists());return r
 def pristine(self):
  self.assertEqual(self.mm.read_bytes(),self.orig);self.assertFalse(self.real.exists());self.assertFalse(self.libdir.exists())
 def test_check_no_changes(self):self.ok('CHECK');self.pristine();self.assertFalse((self.app/'root/mhi2-audio-bt').exists())
 def test_install_restore(self):
  self.ok('INSTALL_PREVIEW');self.assertEqual((self.p/'ACTION.txt').read_text(),'CHECK\n')
  self.assertEqual(self.mm.read_bytes(),(self.p/'payload/mm-renderer-shim.sh').read_bytes());self.assertEqual(self.real.read_bytes(),self.orig)
  for n in LIBS:self.assertEqual((self.libdir/n).read_bytes(),(self.p/'payload'/n).read_bytes())
  self.assertEqual((self.app/'root/mhi2-audio-bt/baseline/mm-renderer').read_bytes(),self.orig)
  self.ok('CHECK');self.ok('RESTORE');self.pristine();self.ok('CHECK')
 def test_reinstall_after_restore_requires_archive(self):
  self.ok('INSTALL_PREVIEW');self.ok('RESTORE');r=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(r.returncode,0);self.assertIn('Restored backup preserved',r.stdout);self.pristine()
 def test_double_install_rejected(self):
  self.ok('INSTALL_PREVIEW');r=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(r.returncode,0)
 def test_tamper_rejected(self):
  with (self.p/'payload/libmhi2_apac.so').open('ab') as f:f.write(b'bad')
  self.bad('INSTALL_PREVIEW','Package hash mismatch')
 def test_wrong_renderer_rejected(self):
  self.mm.write_bytes(b'other firmware');self.orig=self.mm.read_bytes()
  r=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(r.returncode,0);self.assertIn('Unsupported core binary',r.stdout);self.assertFalse((self.app/'root/mhi2-audio-bt').exists())
 def test_core_mismatch_rejected(self):
  (self.app/'armle/usr/lib/libnvaudio.so').write_bytes(b'wrong');self.bad('INSTALL_PREVIEW','Unsupported core binary')
 def test_foreign_files_block_install(self):
  self.real.parent.mkdir(parents=True);self.real.write_bytes(b'foreign');self.bad('INSTALL_PREVIEW','Audio files already exist')
 def test_foreign_libdir_blocks_install(self):
  self.libdir.mkdir();self.bad('INSTALL_PREVIEW','Audio files already exist')
 def test_low_space_rejected(self):
  self.executable(self.bin/'df','#!/bin/sh\necho "Filesystem Size Used Avail Capacity Mounted"; echo "test 100000 99000 1000 99% test"\n');self.bad('INSTALL_PREVIEW','free required')
 def test_symlink_renderer_rejected(self):
  self.mm.unlink();os.symlink(self.p/'stock/mm-renderer',self.mm)
  r=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(r.returncode,0);self.assertFalse((self.app/'root/mhi2-audio-bt').exists())
 def test_restore_without_baseline_rejected(self):
  r=self.run_action('RESTORE');self.assertNotEqual(r.returncode,0);self.assertIn('No complete baseline',r.stdout);self.assertEqual(self.mm.read_bytes(),self.orig)
 def test_failure_at_every_activation_can_restore(self):
  for name in LIBS+['real/mm-renderer','sbin/mm-renderer']:
   with self.subTest(failing=name):
    self.executable(self.bin/'mv','#!/bin/sh\ncase "$1" in */'+name+'.audio-new) exit 1 ;; esac\nexec /bin/mv "$@"\n')
    r=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(r.returncode,0,name)
    self.assertTrue((self.app/'root/mhi2-audio-bt/baseline/COMPLETE').exists())
    self.assertTrue(self.mm.exists())   # the stock path is never left without a file
    self.executable(self.bin/'mv','#!/bin/sh\nexec /bin/mv "$@"\n')
    self.ok('RESTORE');self.pristine();shutil.rmtree(self.app/'root/mhi2-audio-bt')
 def test_failure_at_cp_can_restore(self):
  for name in LIBS+['real/mm-renderer']:
   with self.subTest(failing=name):
    self.executable(self.bin/'cp','#!/bin/sh\nfor a in "$@"; do case "$a" in */'+name+'.audio-new) exit 1 ;; esac; done\nexec /bin/cp "$@"\n')
    r=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(r.returncode,0,name)
    self.executable(self.bin/'cp','#!/bin/sh\nexec /bin/cp "$@"\n')
    self.ok('RESTORE');self.pristine();shutil.rmtree(self.app/'root/mhi2-audio-bt')
 def test_interrupted_restore_can_resume(self):
  self.ok('INSTALL_PREVIEW')
  self.executable(self.bin/'rm','#!/bin/sh\ncase "$1" in *libmhi2_aac.so) exit 1 ;; esac\nexec /bin/rm "$@"\n')
  r=self.run_action('RESTORE');self.assertNotEqual(r.returncode,0)
  self.assertEqual(self.mm.read_bytes(),self.orig)   # the renderer was already restored
  self.executable(self.bin/'rm','#!/bin/sh\nexec /bin/rm "$@"\n')
  self.ok('RESTORE');self.pristine()
 def test_unknown_renderer_blocks_restore(self):
  self.ok('INSTALL_PREVIEW');self.mm.write_bytes(b'unknown');r=self.run_action('RESTORE');self.assertNotEqual(r.returncode,0);self.assertIn('Unknown mm-renderer changes',r.stdout)
 def test_package_is_inert_and_independent(self):
  meta=json.loads((self.p/'package.json').read_text());self.assertFalse(meta['advertises_5_1']);self.assertFalse(meta['claims_stock_audio']);self.assertTrue(meta['independent_of_carplay_package'])
  self.assertEqual((self.p/'payload/libmhi2_audio_hook.so').read_bytes(),(ROOT/'artifacts/native-hook-so-bt-inert/libmhi2_audio_hook.so').read_bytes())
  s=(self.p/'scripts/bt.sh').read_text()
  for forbidden in ['smartphone_integrator','lsd.sh','lang_data','dio_manager','mhi2-audio/']:self.assertNotIn(forbidden,s)
 def test_shim_is_fail_open(self):
  # run the shim with a fake REAL: without hook libs and with DISABLE it must exec the real binary without LD_PRELOAD
  shim=(self.p/'payload/mm-renderer-shim.sh').read_text();tmp=Path(tempfile.mkdtemp(prefix='shim-'));self.addCleanup(shutil.rmtree,tmp);lib=tmp/'lib';state=tmp/'state';lib.mkdir();state.mkdir()
  real=tmp/'real-mm';self.executable(real,'#!/bin/sh\necho "PRELOAD=${LD_PRELOAD:-none} ARGS=$*"\n')
  shim=shim.replace('REAL=$LIB/real/mm-renderer','REAL='+str(real)).replace('/mnt/app/eso/lib/mhi2bt',str(lib)).replace('/mnt/app/root/mhi2-audio-bt',str(state))
  sh=tmp/'shim.sh';sh.write_text(shim)
  def run():
   o=subprocess.run(['/bin/sh',str(sh),'-r','x','-k'],text=True,capture_output=True,env={'PATH':os.environ['PATH']});o=o.stdout.strip()+o.stderr.strip();return o
  self.assertEqual(run(),'PRELOAD=none ARGS=-r x -k')
  (lib/'libmhi2_audio_hook.so').write_bytes(b'x');self.assertIn('PRELOAD='+str(lib),run())
  (state/'DISABLE').write_text('');self.assertEqual(run(),'PRELOAD=none ARGS=-r x -k')
  (state/'DISABLE').unlink();real.rename(tmp/'moved');(state/'baseline').mkdir();b=state/'baseline/mm-renderer';shutil.copy2(tmp/'moved',b)
  self.assertEqual(run(),'PRELOAD=none ARGS=-r x -k')   # real copy missing -> saved original, never a preload
if __name__=='__main__':unittest.main()
