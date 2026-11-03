#!/usr/bin/env python3
"""Build exact-baseline M.I.B. preview; never accesses a vehicle."""
import hashlib,io,json,re,shutil,sys,zipfile
from pathlib import Path
AUDIT=Path(__file__).resolve().parents[1]
ROOT=AUDIT.parents[1]
RETURN=AUDIT/'handoff-windows/MHI2_RETURN_20261015-20261015-073857'
sys.path.insert(0,str(AUDIT/'prototype/lang'))
import ldzip,upmix_lang

def sha(p):return hashlib.sha1(p.read_bytes()).hexdigest()
def patch_language(source):
    changed=[];out=io.BytesIO()
    with zipfile.ZipFile(source) as z,zipfile.ZipFile(out,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as dest:
        for e in z.infolist():
            data=z.read(e.filename)
            prefix='resources/HMIToneEvoHigh/strings_'
            if e.filename.startswith(prefix) and e.filename.endswith('.data'):
                strings,flags=ldzip.d_unpack(data);locale=e.filename[len(prefix):-5]
                want=upmix_lang.texts(locale,strings);have=strings[1000:1006]
                if have and any(have) and have!=want:raise ValueError('Occupied tone text slots: '+locale)
                flags=bytearray(flags)
                while len(strings)<1006:strings.append('');flags.append(0)
                for i,value in enumerate(want):strings[1000+i]=value;flags[1000+i]=8
                data=ldzip.d_pack(strings,flags);changed.append(e.filename)
            e.compress_type=zipfile.ZIP_DEFLATED;dest.writestr(e,data)
    assert changed
    with zipfile.ZipFile(source) as a,zipfile.ZipFile(io.BytesIO(out.getvalue())) as b:
        assert a.namelist()==b.namelist() and b.testzip() is None
        for name in a.namelist():
            if name not in changed:assert a.read(name)==b.read(name)
            else:
                old,of=ldzip.d_unpack(a.read(name));new,nf=ldzip.d_unpack(b.read(name))
                assert old==new[:len(old)] and len(new)>=1006
    return out.getvalue(),len(changed)

def main():
    dest=AUDIT/'artifacts/mib-audio-preview'
    if len(sys.argv)>1:dest=Path(sys.argv[1]).resolve()
    if dest.exists():raise SystemExit('Refusing to overwrite existing build')
    dest.mkdir(parents=True)
    shutil.copytree(AUDIT/'prototype/mib',dest,dirs_exist_ok=True)
    p=dest/'mhi2-audio';(p/'payload').mkdir();(p/'stock').mkdir();(p/'target').mkdir()
    source={
        'lsd.sh':RETURN/'evidence/configs/lsd.sh',
        'smartphone_integrator.json':RETURN/'evidence/configs/smartphone_integrator.json',
        'lang-stock.zip':RETURN/'evidence/localization/binaries/lang_data.zip',
    }
    for name,path in source.items():shutil.copy2(path,p/'stock'/name)
    # Preserve all existing launch behavior; prepend only this jar.
    text=source['lsd.sh'].read_text()
    needle='## Launch J9\n';assert text.count(needle)==1
    text=text.replace(needle,needle+'BOOTCLASSPATH="/mnt/app/eso/hmi/lsd/jars/mhi2_audio_ui.jar:$BOOTCLASSPATH"\n')
    (p/'payload/lsd.sh').write_text(text)
    text=source['smartphone_integrator.json'].read_text()
    start=text.index('"carplay":{');a=text.index('"envs":[',start)+len('"envs":[');b=text.index(']',a)
    assert '"exec":"dio_manager"' in text[start:a] and 'LD_PRELOAD=' not in text[a:b]
    text=text[:a]+'\n                "LD_PRELOAD=/mnt/app/eso/lib/libmhi2_audio.so",'+text[a:]
    (p/'payload/smartphone_integrator.json').write_text(text)
    for variant in ['stock']:
        data,count=patch_language(source['lang-'+variant+'.zip']);(p/'payload'/('lang-'+variant+'.zip')).write_bytes(data)
        print('Patched language tables',variant,count,'bytes',len(data))
    shutil.copy2(AUDIT/'artifacts/native-probe/libmhi2_audio.so',p/'payload/libmhi2_audio.so')
    shutil.copy2(AUDIT/'artifacts/java/mhi2_audio_ui.jar',p/'payload/mhi2_audio_ui.jar')
    pairs={
        'eso/lib/libairplay.so':RETURN/'evidence/binaries/libairplay.so',
        'armle/usr/lib/libnvaudio.so':RETURN/'evidence/binaries/libnvaudio.so',
        'root/bin-target/deva-nvaudio':RETURN/'evidence/binaries/deva-nvaudio',
        'eso/bin/apps/smartphone_integrator':RETURN/'evidence/binaries/smartphone_integrator',
        'eso/bin/apps/mediarouter':RETURN/'evidence/binaries/mediarouter',
        'eso/bin/apps/dio_manager':RETURN/'evidence/binaries/dio_manager',
    }
    (p/'target/core.sha1').write_text(''.join(sha(path)+'  '+rel+'\n' for rel,path in pairs.items()))
    (p/'HMI.sha1').write_text(sha(RETURN/'evidence/localization/binaries/lsd.jxe')+'\n')
    (p/'ACTION.txt').write_text('CHECK\n')
    files=sorted(q for folder in ['payload','stock','target','scripts'] for q in (p/folder).rglob('*') if q.is_file())+[p/'HMI.sha1']
    (p/'SHA1SUMS').write_text(''.join(sha(q)+'  '+str(q.relative_to(p))+'\n' for q in files))
    (p/'package.json').write_text(json.dumps({'status':'EXPERIMENTAL_PREVIEW','train':'MHI2_ER_AUG22_K3346','mu':'1438','default_action':'CHECK','default_level':'OFF','applies':'next CarPlay stream configuration; reconnect recommended','apac_type103':'NOT_INTEGRATED','lfe_assist':False,'physical_rcc_six_channels':'NOT_VALIDATED','unit_identity_included':False,'module':'audio','module_dependencies':[],'language_baseline':'stock_only'},indent=2)+'\n')
    shutil.copy2(AUDIT/'upstream/LICENSE',p/'LICENSE')
    shutil.copy2(AUDIT/'upstream/NOTICE.md',p/'NOTICE.md')
    (dest/'README.md').write_text('''# MHI2 K3346 audio — EXPERIMENTAL PREVIEW

Пакет для M.I.B.; початковий ACTION.txt=CHECK. Розпакувати вміст у коріньSD1: mod/custom.sh і mhi2-audio/ поруч. З GEM/M.I.B. запустити custom.sh як для пакета української локалізації; зовнішнє Finished не є доказом успіху. Перевірити exitcode і mhi2-audio/logs.

Дії: CHECK; INSTALL_PREVIEW; RESTORE. Для дії змінити один рядок у mhi2-audio/ACTION.txt. Перед будь-якою зміною app даних дія повертається у CHECK. Скрипт не робить reboot і не вбиває процеси. Перед INSTALL_PREVIEW/RESTORE від’єднати iPhone/CarPlay; зміни набирають чинності після ручного перезапуску власником.

Створюється CarPlay UpMix у Sound→Sound effects. DefaultOff. Off/Moderate/Normal/Full/Aggressive зберігаються HMI. У цьому preview зміна застосовується при наступній конфігурації потоку: перепідключити CarPlay. Це самостійний аудіомодуль для зчитаних штатних ресурсів. Українська локалізація не потрібна і не входить у пакет; інші модифікації ресурсів відхиляються. Датасети, мовний вибір, IFS та оригінальні core libraries не змінюються.

Це не завершена автомобільна валідація. UpMix: L/R/Ls/Rs,C/LFEzero; LFEAssist і B&O calibration вимкнені. APAC decoder перевірений окремо, але type103 negotiation не входить у цей пакет. RCC physical mapping, реальні telephone/focus, навантаженняTegra і апаратний restore ще не підтверджені. Спочатку потрібен огляд цього preview й контрольована перевірка; не називати готовим production5.1.

Backup: /mnt/app/root/mhi2-audio/baseline. Не видаляти його. RESTORE використовує оригінали цього встановлення; сторонні змінені файли відхиляються. Тримати ту самуSD для recovery. Пакет приватний для технічної перевірки, без vehicle logs/credentials/ідентифікаторів.\n''')
    archive=dest.with_suffix('.zip')
    if archive.exists():raise SystemExit('Refusing existing archive')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for q in sorted(dest.rglob('*')):
            if q.is_file():z.write(q,str(q.relative_to(dest)))
    with zipfile.ZipFile(archive) as z:assert z.testzip() is None
    print('Built',archive)
if __name__=='__main__':main()
