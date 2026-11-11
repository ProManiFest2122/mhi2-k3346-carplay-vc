#!/usr/bin/env python3
"""Build the INERT Bluetooth/USB/SD audio hook M.I.B. draft package; never accesses a vehicle.
Hook = HOOK_SO_LOCAL=1 HOOK_SO_QUALIFIED=0 build (claims no stock audio). Independent of the CarPlay package."""
import hashlib,json,shutil,subprocess,sys,zipfile
from pathlib import Path
AUDIT=Path(__file__).resolve().parents[1]
ROOT=AUDIT.parents[1]
RETURN=AUDIT/'handoff-windows/MHI2_RETURN_20261015-20261015-073857'
MM_SHA256='6f8e361352b3e07419ca371c7d3ee9cd214dc82b60b4096721bbb3d8f5e2fec9'  # unit binary == firmware copy (session 4)
def sha(p):return hashlib.sha1(p.read_bytes()).hexdigest()
def main():
    dest=AUDIT/'artifacts/mib-audio-bt-draft-v3'
    if len(sys.argv)>1:dest=Path(sys.argv[1]).resolve()
    if dest.exists():raise SystemExit('Refusing to overwrite existing build')
    hook=AUDIT/'artifacts/native-hook-so-bt-inert/libmhi2_audio_hook.so'
    if not hook.is_file():raise SystemExit('Build the inert BT hook first: HOOK_SO_LOCAL=1 HOOK_SO_QUALIFIED=0 python3 scripts/build_native_hook_so.py')
    dest.mkdir(parents=True)
    shutil.copytree(AUDIT/'prototype/mib-hook-bt',dest,dirs_exist_ok=True)
    p=dest/'mhi2-audio-bt';(p/'stock').mkdir();(p/'target').mkdir()
    img=ROOT/'extract/MMX2/app/50/default/app.img'
    data=subprocess.check_output([sys.executable,str(ROOT/'tools/qnx6.py'),str(img),'cat','/armle/usr/sbin/mm-renderer'])
    if hashlib.sha256(data).hexdigest()!=MM_SHA256:raise SystemExit('mm-renderer firmware copy differs from the unit binary')
    (p/'stock/mm-renderer').write_bytes(data)
    for name,path in {'libmhi2_audio_hook.so':hook,'libmhi2_apac.so':AUDIT/'artifacts/native-type103/libmhi2_apac.so','libmhi2_aac.so':AUDIT/'artifacts/fdk-decoder-qnx/libmhi2_aac.so','libmhi2_fdk.so':AUDIT/'artifacts/fdk-decoder-qnx/libmhi2_fdk.so'}.items():shutil.copy2(path,p/'payload'/name)
    shutil.copy2(AUDIT/'artifacts/fdk-decoder-qnx/NOTICE',p/'FDK-NOTICE')
    pairs={'armle/usr/sbin/mm-renderer':p/'stock/mm-renderer','armle/usr/lib/libnvaudio.so':RETURN/'evidence/binaries/libnvaudio.so','root/bin-target/deva-nvaudio':RETURN/'evidence/binaries/deva-nvaudio'}
    (p/'target/core.sha1').write_text(''.join(sha(path)+'  '+rel+'\n' for rel,path in pairs.items()))
    (p/'HMI.sha1').write_text(sha(RETURN/'evidence/localization/binaries/lsd.jxe')+'\n')
    (p/'ACTION.txt').write_text('CHECK\n')
    files=[p/'payload'/n for n in ['libmhi2_audio_hook.so','libmhi2_apac.so','libmhi2_aac.so','libmhi2_fdk.so','mm-renderer-shim.sh']]+[p/'stock/mm-renderer',p/'target/core.sha1',p/'scripts/bt.sh',p/'HMI.sha1']
    (p/'SHA1SUMS').write_text(''.join(sha(q)+'  '+str(q.relative_to(p))+'\n' for q in files))
    (p/'package.json').write_text(json.dumps({'status':'DRAFT_INERT_BT_HOOK','hook_variant':'MHI2_NV_SIX_CHANNEL_QUALIFIED=0 + MHI2_NV_BRIDGE_LOCAL_MEDIA','advertises_5_1':False,'claims_stock_audio':False,'train':'MHI2_ER_AUG22_K3346','mu':'1438','default_action':'CHECK','touches':['/mnt/app/armle/usr/sbin/mm-renderer (exec shim)','/mnt/app/eso/lib/mhi2bt/real/mm-renderer','/mnt/app/eso/lib/mhi2bt/'],'independent_of_carplay_package':True,'shim_spawn_by_medialauncher':'NOT_VALIDATED','unit_identity_included':False},indent=2)+'\n')
    shutil.copy2(AUDIT/'upstream/LICENSE',p/'LICENSE');shutil.copy2(AUDIT/'upstream/NOTICE.md',p/'NOTICE.md')
    (dest/'README.md').write_text('''# MHI2 K3346 audio BT/USB/SD hook - DRAFT, INERT

Чернетка окремого пакета для Bluetooth/USB/SD (mm-renderer). Не залежить від CarPlay-пакета. Hook зібрано з MHI2_NV_SIX_CHANNEL_QUALIFIED=0: завантажується, але нічого не перехоплює і не змінює звук.

Розпакувати mod/custom.sh і mhi2-audio-bt/ у корінь SD1. Дії: CHECK (за замовчуванням), INSTALL_PREVIEW, RESTORE: змінити один рядок у mhi2-audio-bt/ACTION.txt. Без reboot; результат у mhi2-audio-bt/logs.

Інсталятор: зберігає оригінальний mm-renderer у /mnt/app/root/mhi2-audio-bt/baseline і як mhi2bt/real/mm-renderer (та сама назва файлу); кладе бібліотеки в /mnt/app/eso/lib/mhi2bt; останнім ставить shell-шим на місце mm-renderer (шим сам fail-open: без бібліотек або з файлом /mnt/app/root/mhi2-audio-bt/DISABLE запускає оригінал). Ефект після наступного старту mm-renderer. RESTORE повертає оригінал.

Не перевірено в авто: чи medialauncher запускає shell-скрипт замість бінарника, M.I.B. CHECK/INSTALL/RESTORE на QNX-пристрої, завантаження hook у справжній mm-renderer. Перед будь-яким записом потрібні бекап і telnet (адаптер AX88772B).
''')
    archive=dest.with_suffix('.zip')
    if archive.exists():raise SystemExit('Refusing existing archive')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for q in sorted(dest.rglob('*')):
            if q.is_file():z.write(q,str(q.relative_to(dest)))
    with zipfile.ZipFile(archive) as z:assert z.testzip() is None
    print('Built',archive)
if __name__=='__main__':main()
