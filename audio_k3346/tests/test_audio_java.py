import os,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
JDK=ROOT.parents[1]/'runtime/k3346-decomp/tc/zulu8.96.0.205-ca-jdk8.0.504-macosx_aarch64/Contents/Home'
class JavaSettingTest(unittest.TestCase):
 def test_persistence_defaults_and_selection(self):
  with tempfile.TemporaryDirectory(prefix='mhi2-audio-java-') as name:
   work=Path(name)
   sources={
    'de/audi/atip/base/IFrameworkAccess.java':'package de.audi.atip.base; public interface IFrameworkAccess { de.audi.atip.storage.IStorageAccess getStorageMgr(); }',
    'de/audi/atip/storage/IStorageAccess.java':'package de.audi.atip.storage; public interface IStorageAccess { int getInt(int n,int k,int d); void setInt(int n,int k,int v); }',
    'de/audi/tghu/storage/FlushValuesDict.java':'package de.audi.tghu.storage; public final class FlushValuesDict { public static int[][] values={{1,2,3}}; }',
    'com/luka/carplay/tone/UpmixToneUi.java':'package com.luka.carplay.tone; public final class UpmixToneUi {public static int shown=-1;public static void show(int v){shown=v;}}',
   }
   source=(ROOT/'prototype/java/src/com/luka/carplay/core/UpmixSetting.java').read_text().replace('/tmp/mhi2_audio_level',str(work/'level')).replace('/mnt/app/root/mhi2-audio/FIRST_BOOT',str(work/'FIRST_BOOT'))
   sources['com/luka/carplay/core/UpmixSetting.java']=source
   (work/'FIRST_BOOT').write_text('FIRST_BOOT_OFF')
   sources['Test.java']='''
import com.luka.carplay.core.UpmixSetting;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.storage.IStorageAccess;
import de.audi.tghu.storage.FlushValuesDict;
import java.io.*;
public class Test implements IFrameworkAccess,IStorageAccess {
 int stored=3,writes;boolean fail;
 public IStorageAccess getStorageMgr(){return this;}
 public int getInt(int n,int k,int d){return stored;}
 public void setInt(int n,int k,int v){if(fail)throw new RuntimeException("storage unavailable");stored=v;writes++;}
 static void require(boolean v){if(!v)throw new RuntimeException("assert");}
 public static void main(String[] a)throws Exception {
  Test f=new Test();UpmixSetting.boot(f);
  require(UpmixSetting.available()&&UpmixSetting.current()==0&&f.stored==0&&f.writes==1);
  require(FlushValuesDict.values.length==2&&FlushValuesDict.values[0][0]==1);
  require(!new File(a[0]+"/FIRST_BOOT").exists()&&new File(a[0]+"/FIRST_BOOT_APPLIED").exists());
  for(int v=0;v<5;v++) {UpmixSetting.select(v);require(f.stored==v&&UpmixSetting.current()==v);FileInputStream in=new FileInputStream(a[0]+"/level");require(in.read()=='0'+v&&in.read()==10&&in.read()==-1);in.close();}
  int writes=f.writes;UpmixSetting.select(-1);UpmixSetting.select(5);require(f.writes==writes&&UpmixSetting.current()==4);
  f.fail=true;boolean caught=false;try{UpmixSetting.select(1);}catch(RuntimeException ex){caught=true;}require(caught&&UpmixSetting.current()==4);
  System.out.println("PASS: first boot Off, flush list preserved, five persisted choices, invalid/storage-error isolation");
 }
}'''
   paths=[]
   for rel,text in sources.items():
    path=work/rel;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text);paths.append(str(path))
   subprocess.run([str(JDK/'bin/javac'),'-d',str(work),*paths],check=True,capture_output=True,text=True)
   result=subprocess.run([str(JDK/'bin/java'),'-cp',str(work),'Test',str(work)],check=True,capture_output=True,text=True)
   self.assertIn('PASS:',result.stdout)
if __name__=='__main__':unittest.main()
