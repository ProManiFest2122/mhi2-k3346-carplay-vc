# M.I.B. audio hook: чернетка пакета (INERT) — підсумок 15.10.2026

Пакет: `artifacts/mib-audio-hook-draft.zip` (sha256 у `.zip.sha256`), збирається `python3 scripts/build_mib_audio_hook.py` після
`HOOK_SO_QUALIFIED=0 python3 scripts/build_native_hook_so.py`. Старий `mib-audio-standalone-preview.zip` лишається історичним і для 5.1 не використовується.
Це чернетка для перевірки ІНСТАЛЯЦІЇ, не увімкнення 6 каналів: hook зібрано з `MHI2_NV_SIX_CHANNEL_QUALIFIED=0` (fail-closed).

## Що робить / не робить
- Ставить у `/mnt/app/eso/lib`: `libmhi2_audio_hook.so`, `libmhi2_apac.so`, `libmhi2_aac.so`, `libmhi2_fdk.so`; JAR з рядком CarPlay UpMix; змінені `lsd.sh`, `smartphone_integrator.json`
  (`LD_PRELOAD` і `AUDIO_SPATIAL_CODEC_LIB` лише для dio_manager) і `lang_data.zip` (шість рядків UpMix). Датасети (`0x3000`/`0x3B00` тощо), IFS, мовний вибір, оригінальні core binaries не змінюються.
- Звук лишається штатним: hook не рекламує 5.1, не вмикає spatial/realtime PCM і не перехоплює штатний потік навіть при виставлених `AUDIO_*` (host-тест `scripts/test_nv_inert.sh`, емуляторний прогін із inert `.so`).
- Рядок UpMix у меню буде, але на звук не впливає.

## Перевірено (host/емулятор) і не перевірено
- Host-симуляція інсталятора, 12 тестів PASS (`tests/test_mib_audio_hook.py`): CHECK без змін, install/restore, відмова при підміні файлу/чужій бібліотеці/невідомому core/підключеному телефоні/нестачі місця,
  відмова на КОЖНОМУ з 8 кроків активації з подальшим чистим RESTORE, повторна інсталяція після RESTORE заборонена без архівування.
- НЕ перевірено: виконання на QNX-голові (M.I.B. CHECK/INSTALL/RESTORE), завантаження hook у справжній dio_manager (в емуляторі dio_manager не стартує: немає libcomm.so), розв'язання NEEDED-бібліотек через LD_LIBRARY_PATH
  dio_manager, вільне місце на app-розділі. Усі ці пункти потребують авто й явної згоди the owner.

## Порядок на SD1 (лише після окремого рішення the owner; без доступу до авто нічого не виконується)
1. Розпакувати `mod/custom.sh` і `mhi2-audio/` у корінь SD1 поряд з M.I.B.; якщо `custom.sh` зайнятий, зберегти копію.
2. `mhi2-audio/ACTION.txt` = `CHECK`; запустити custom.sh; прочитати `mhi2-audio/logs/run-*.log` (потрібен CHECK PASS і exit 0, а не напис Finished від M.I.B.).
3. Для встановлення: від'єднати iPhone, стабільне живлення, у ACTION.txt `INSTALL_PREVIEW`, повторити; backup у `/mnt/app/root/mhi2-audio/baseline` не видаляти. Перезавантаження вручну власником.
4. Помилка під час мутації: не перезавантажувати, `RESTORE` тим самим пакетом.
Перед будь-яким записом дотримуватись `analysis/03-RUNBOOK.md` (telnet-канал відновлення, базова копія) і skill `mib-shell`.
