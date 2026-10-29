#!/usr/bin/env python3
"""Build the INERT (fail-closed) audio hook M.I.B. draft package; never accesses a vehicle.
The hook .so is the MHI2_NV_SIX_CHANNEL_QUALIFIED=0 build: it claims no stock audio and advertises no 5.1."""
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
    dest=AUDIT/'artifacts/mib-audio-hook-draft-v2'
    if len(sys.argv)>1:dest=Path(sys.argv[1]).resolve()
    if dest.exists():raise SystemExit('Refusing to overwrite existing build')
    dest.mkdir(parents=True)
    shutil.copytree(AUDIT/'prototype/mib-hook',dest,dirs_exist_ok=True)
    p=dest/'mhi2-audio';(p/'payload').mkdir();(p/'stock').mkdir();(p/'target').mkdir()
    source={
        'lsd.sh':RETURN/'evidence/configs/lsd.sh',
        'smartphone_integrator.json':RETURN/'evidence/configs/smartphone_integrator.json',
        'lang-stock.zip':RETURN/'evidence/localization/binaries/lang_data.zip',
        # The Ukrainian package's installed lang_data.zip (pinned by hash in SHA1SUMS): audio may be installed after it.
        'lang-uk.zip':ROOT/'publication/mhi2-ukrainian-localization/sd-card/uk-localization/payload/lang_data.zip',
    }
    for name,path in source.items():shutil.copy2(path,p/'stock'/name)
    # lsd.sh and smartphone_integrator.json are NOT shipped as payload: the stock lsd.sh already loads every
    # lsd/jars/*.jar, and scripts/json.awk edits the carplay envs of the live JSON in place (stock/ copies are
    # reference inputs: lsd.sh is a gate, the JSON is the test baseline).
    for variant in ['stock','uk']:
        data,count=patch_language(source['lang-'+variant+'.zip']);(p/'payload'/('lang-'+variant+'.zip')).write_bytes(data)
        print('Patched language tables',variant,count,'bytes',len(data))
    inert=AUDIT/'artifacts/native-hook-so-inert/libmhi2_audio_hook.so'
    if not inert.is_file():raise SystemExit('Build the inert hook first: HOOK_SO_QUALIFIED=0 python3 scripts/build_native_hook_so.py')
    for name,path in {'libmhi2_audio_hook.so':inert,'libmhi2_apac.so':AUDIT/'artifacts/native-type103/libmhi2_apac.so','libmhi2_aac.so':AUDIT/'artifacts/fdk-decoder-qnx/libmhi2_aac.so','libmhi2_fdk.so':AUDIT/'artifacts/fdk-decoder-qnx/libmhi2_fdk.so'}.items():shutil.copy2(path,p/'payload'/name)
    shutil.copy2(AUDIT/'artifacts/fdk-decoder-qnx'/'NOTICE',p/'FDK-NOTICE')
    ui=AUDIT/'artifacts/java/mhi2_audio_ui-v46.jar'   # class-file major 46 like every stock/known-working patch jar (scripts/downgrade_jar_major.py)
    if not ui.is_file():raise SystemExit('Build the v46 UI jar first: python3 scripts/downgrade_jar_major.py artifacts/java/mhi2_audio_ui.jar artifacts/java/mhi2_audio_ui-v46.jar')
    shutil.copy2(ui,p/'payload/mhi2_audio_ui.jar')
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
    (p/'package.json').write_text(json.dumps({'status':'DRAFT_INERT_HOOK','hook_variant':'MHI2_NV_SIX_CHANNEL_QUALIFIED=0','advertises_5_1':False,'claims_stock_audio':False,'train':'MHI2_ER_AUG22_K3346','mu':'1438','default_action':'CHECK','default_level':'OFF','applies':'next CarPlay stream configuration; reconnect recommended','apac_type103':'LOADED_BUT_DISABLED','lfe_assist':False,'physical_rcc_six_channels':'NOT_VALIDATED','unit_identity_included':False,'module':'audio','module_dependencies':[],'language_baseline':'stock_or_ukrainian_package','language_install_order':'ukrainian first; RESTORE audio before RESTORE Ukrainian','config_edit':'in_place_awk','lsd_sh_modified':False},indent=2)+'\n')
    shutil.copy2(AUDIT/'upstream/LICENSE',p/'LICENSE')
    shutil.copy2(AUDIT/'upstream/NOTICE.md',p/'NOTICE.md')
    (dest/'README.md').write_text('''# MHI2 K3346 audio hook — DRAFT, INERT

Чернетка пакета для M.I.B. (лише для перевірки встановлення). Hook зібрано з MHI2_NV_SIX_CHANNEL_QUALIFIED=0: він завантажується у dio_manager, але не рекламує 5.1, не вмикає spatial/realtime PCM і не перехоплює штатний звук навіть якщо в оточенні виставлено AUDIO_* змінні. Рядок CarPlay UpMix у меню лишається, але на звук не впливає. Нічого не вмикає 6 каналів; RCC-gate не закрито.

Розпакувати mod/custom.sh і mhi2-audio/ у корінь SD1. Дії: CHECK (за замовчуванням); INSTALL_PREVIEW; RESTORE; змінити один рядок у mhi2-audio/ACTION.txt. Перед INSTALL/RESTORE від'єднати iPhone/CarPlay; без reboot; результат у mhi2-audio/logs (exit code, не зовнішній Finished).

Встановлює в /mnt/app/eso/lib: libmhi2_audio_hook.so, libmhi2_apac.so, libmhi2_aac.so, libmhi2_fdk.so (FDK AAC, див. FDK-NOTICE), UI JAR, lang_data.zip (шість рядків UpMix) і два рядки в envs запису carplay файлу smartphone_integrator.json (LD_PRELOAD додається через двокрапку до наявного, якщо інший компонент уже його задав; AUDIO_SPATIAL_CODEC_LIB). lsd.sh не змінюється: штатний lsd.sh сам додає lsd/jars/*.jar. RESTORE прибирає лише ці рядки. Порядок: спершу CarPlay-основа, аудіо останнім; якщо основу перевстановлюють, її awk стирає рядок LD_PRELOAD, тоді аудіо ставити знову. Якщо український пакет уже встановлено, його lang_data.zip приймається як базовий (варіант uk) і до нього додаються ті самі шість рядків UpMix; тоді RESTORE аудіо робити ДО RESTORE українізації. Датасети, IFS, мовний вибір, оригінальні core binaries не змінюються. Backup: /mnt/app/root/mhi2-audio/baseline, не видаляти; RESTORE повертає оригінали й видаляє лише власні файли.

Не перевірено в авто: завантаження hook у справжній dio_manager, M.I.B. CHECK/INSTALL/RESTORE на QNX-пристрої, NEEDED-бібліотеки через LD_LIBRARY_PATH dio_manager. Вмикання 6 каналів потребує окремої збірки (QUALIFIED=1) після закриття RCC-gate і окремого рішення власника.\n''')

    archive=dest.with_suffix('.zip')
    if archive.exists():raise SystemExit('Refusing existing archive')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for q in sorted(dest.rglob('*')):
            if q.is_file():z.write(q,str(q.relative_to(dest)))
    with zipfile.ZipFile(archive) as z:assert z.testzip() is None
    print('Built',archive)
if __name__=='__main__':main()
