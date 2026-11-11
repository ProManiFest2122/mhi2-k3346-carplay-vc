/* K3346 standalone audio UI. No dependency on the full CarPlay application. */
package com.luka.carplay.core;
import com.luka.carplay.tone.UpmixToneUi;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.tghu.storage.FlushValuesDict;
import java.io.File;
import java.io.FileOutputStream;
public final class UpmixSetting {
 private static final int NAMESPACE=1009, KEY=1000;
 private static IFrameworkAccess framework;
 private static boolean booted;
 private static volatile int currentLevel;
 private UpmixSetting() {}
 public static synchronized void boot(IFrameworkAccess f) {
  if(booted)return;
  framework=f;
  /* Extend the live stock flush list rather than replace the global class. */
  int[][] old=FlushValuesDict.values;boolean found=false;
  for(int i=0;i<old.length;i++)if(old[i][0]==NAMESPACE&&old[i][1]==KEY)found=true;
  if(!found){int[][] next=new int[old.length+1][];System.arraycopy(old,0,next,0,old.length);next[old.length]=new int[]{NAMESPACE,KEY,0};FlushValuesDict.values=next;}
  int value=f.getStorageMgr().getInt(NAMESPACE,KEY,0);
  if(value<0||value>4)value=0;
  File first=new File("/mnt/app/root/mhi2-audio/FIRST_BOOT");
  if(first.exists()){value=0;f.getStorageMgr().setInt(NAMESPACE,KEY,0);}
  publish(value);
  if(first.exists()&&!first.renameTo(new File("/mnt/app/root/mhi2-audio/FIRST_BOOT_APPLIED")))throw new RuntimeException("Cannot seal first boot Off");
  currentLevel=value;UpmixToneUi.show(value);booted=true;
 }
 public static int current(){return currentLevel;}
 public static boolean available(){return booted;}
 public static synchronized void select(int value){
  if(!booted||value<0||value>4)return;
  /* Write storage first; a failure never activates a different runtime level. */
  framework.getStorageMgr().setInt(NAMESPACE,KEY,value);
  publish(value);currentLevel=value;
 }
 private static void publish(int value){
  File temp=new File("/tmp/mhi2_audio_level.new");File target=new File("/tmp/mhi2_audio_level");
  try{
   FileOutputStream out=new FileOutputStream(temp);
   try{out.write(new byte[]{(byte)('0'+value),10});}finally{out.close();}
   if(!temp.renameTo(target))throw new java.io.IOException("level rename failed");
  }catch(java.io.IOException ex){throw new RuntimeException("Cannot publish audio level: "+ex);}
 }
}
