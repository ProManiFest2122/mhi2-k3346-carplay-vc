"""Host filesystem simulation only; never executes on a vehicle."""
import hashlib,os,shutil,subprocess,tempfile,unittest,re,shlex
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PACKAGE=Path(os.environ.get('MHI2_AUDIO_TEST_PACKAGE',str(ROOT/'artifacts/mib-audio-standalone-preview')))
RETURN=ROOT/'handoff-windows/MHI2_RETURN_20261015-20261015-073857'

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
  s=s.replace('hash /ifs/lsd.jxe','hash '+shlex.quote(str(self.hmi)))
  script.write_text(s)
  lines=(self.p/'SHA1SUMS').read_text().splitlines();lines=[hashlib.sha1(script.read_bytes()).hexdigest()+'  scripts/audio.sh' if line.endswith('  scripts/audio.sh') else line for line in lines];(self.p/'SHA1SUMS').write_text('\n'.join(lines)+'\n')
 def tearDown(self):
  shutil.rmtree(self.root)
 def executable(self,p,s):p.write_text(s);p.chmod(0o755)
 def run_action(self,action):
  (self.p/'ACTION.txt').write_text(action+'\n')
  return subprocess.run(['/bin/sh',str(self.p/'scripts/audio.sh')],text=True,capture_output=True,timeout=40)
 def unchanged(self):
  for q,b in self.orig.items():self.assertEqual(q.read_bytes(),b)
 def test_check_no_app_changes(self):
  result=self.run_action('CHECK');self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.unchanged();self.assertFalse((self.app/'root/mhi2-audio').exists())
 def test_install_restore_stock(self):
  result=self.run_action('INSTALL_PREVIEW');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
  self.assertEqual((self.p/'ACTION.txt').read_text(),'CHECK\n');self.assertTrue((self.app/'root/mhi2-audio/FIRST_BOOT').is_file())
  self.assertIn('libmhi2_audio.so',self.config.read_text());self.assertEqual((self.lsd/'jars/lang_data.zip').read_bytes(),(self.p/'payload/lang-stock.zip').read_bytes())
  result=self.run_action('CHECK');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
  result=self.run_action('RESTORE');self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.unchanged()
  self.assertFalse((self.app/'eso/lib/libmhi2_audio.so').exists());self.assertFalse((self.lsd/'jars/mhi2_audio_ui.jar').exists())
  result=self.run_action('CHECK');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
 def test_tamper_rejected_before_mutation(self):
  with (self.p/'payload/libmhi2_audio.so').open('ab') as f:f.write(b'bad')
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
 def test_partial_install_can_restore(self):
  body='#!/bin/sh\ncase "$2" in *smartphone_integrator.json.audio-new) exit 1 ;; esac\nexec /bin/mv "$@"\n'
  # Fail the last activation after native/UI/language/startup already changed.
  self.executable(self.bin/'mv',body.replace('case "$2"','case "$1"'))
  result=self.run_action('INSTALL_PREVIEW');self.assertNotEqual(result.returncode,0)
  self.assertTrue((self.app/'root/mhi2-audio/baseline/COMPLETE').exists())
  self.executable(self.bin/'mv','#!/bin/sh\nexec /bin/mv "$@"\n')
  result=self.run_action('RESTORE');self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.unchanged()
if __name__=='__main__':unittest.main()
