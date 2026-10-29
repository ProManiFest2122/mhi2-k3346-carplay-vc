# Повне перенесення аудіо: актуальний стан

Власник просить повну аудіоповедінку референсу та самостійний M.I.B. модуль. Робота не завершена. Нового повного пакета для авто ще немає. Попередній `mib-audio-standalone-preview.zip` НЕ використовувати для перевірки повного 5.1: у ньому відсутня адаптація внутрішнього маршруту RCC, а шість каналів на MMX не доводять їх передачі на динаміки.

## Зафіксований референс

https://github.com/luka-dev/mib2q-carplay-rgi, HEAD перевірено через ls-remote: `74b0d70d7cae89061cd586f854bf2cc68c8fd50f`. Відтворюваний експорт у `reference-audio-74b0d70/`: audio, framework, APAC, mask model, відповідні тести/документи та jpeg_safety як залежність загального audio test runner. Трековані джерела не змінені. Власні зміни — scripts/prototype та generated port artifacts.

## Не пропускаємо жодної поточної аудіофункції

| Поведінка референсу | Поточний доказ / стан K3346 |
|---|---|
| /info type103, APAC65 + AAC23, mainBuffered у SETUP | Повний reference receiver збережений; host interposer contract tests PASS. K3346 private profile статично відновлено; живий receiver/native NVIDIA sink ще не інтегровані. |
| APAP framing, authentication/decryption, обидва AAD, cookies/TLV/trim, APAC PCM6 reorder | Не лише decoder: повний reference numeric pipeline пройшов ARM/QNX, run090830. |
| Inline cookie changes, AAC↔APAC, geometry-selected render, 44.1/48k | Reference source та host tests PASS; власний48k AAC backend/table пройшов QNX canary (нижче). Live switching і44.1k target ще не інтегровані. |
| 5.1 discrete channels | PCM6 reorder перевірений; NVIDIA driver/MMX six slots перевірені раніше; фізичний RCC internal route не готовий. |
| 5.1.2 APAC / fold height до PCM6 | Source і host downmix tests PASS; фізична схема має шість виходів, не окремі height speakers. |
| Повний SCENE variant J, mask net c, clustering/duck/instrument stage | Невкорочений SCENE + справжній 32KiB model пройшли ARM/QNX run090448. Це numerical reference B&O profile, не готовий профіль внутрішньої акустики. |
| П'ять рівнів, live 300ms fades, stream reset | Full native reference DSP PASS; попередній встановлюваний preview читає рівень лише при configure, тому live control до target runtime ще треба з'єднати. |
| Async mask worker / stale-generation isolation / sync fallback | Source збережений, host suite PASS. Окремий ARM worker/lifecycle тест ще не виконаний; native full SCENE probe використовує synchronous reference. |
| LFE V2, headroom, cabin guard | Повністю збережені у reference source/host tests. Профіль прив'язаний до J525; не вмикати його gain/matrix на internal amp без адаптації до нашого SignalFlow31/3000. Native SCENE probe працює з LFE assist Off. |
| SoundCheck, metadata ramps; optional RMS/peak guards | Reference host tests PASS; target receiver integration pending. Не вмикати лабораторні guards як заміну SoundCheck. |
| Pause/resume, FLUSHBUFFERED, seek, sequence gaps, anchors/feedback, detached/cold playback | Reference lifecycle/clock/interposer suites PASS. QSA recovery не є NVIDIA recovery; потрібен власний sink adapter. |
| Type100 stereo UpMix, voice/Siri/phone priority, stream ownership/reconnect | Reference concurrency/control tests PASS; попередній NVIDIA fixture не має повного lifecycle/concurrency contract. Новий target runtime ще не підключений. |
| MMI row, persistence, control delivery | JAR Java1.4/host logic PASS раніше; візуальний MMI та live bus/settings-to-runtime на K3346 ще не підтверджені. Модуль не залежить від локалізації чи інших CarPlay features. |
| M.I.B. CHECK/install/backup/restore | Старий preview installer host tests7/7 PASS; нові RCC зміни не можна додавати до нього до завершення точного профілю та rollback. |

Ретроспективні Universal/HSSR моделі, retired у референсі, не є поточними production effects; вони не підміняють SCENE.

## Нові ARM/QNX результати

`artifacts/runs/20261015-090448/full-scene-analysis.json`: 98304 frames, model accepted, усі5 live levels, C/LFEzero, reset silence, guestexit0. ARM energy `[226610891,389859450,0,0,331822980,134360663]`, host `[226610894,389859450,0,0,331823000,134360642]`. Gate — агрегати max(100,host*1e-5); це не sample-exact порівняння й не Tegra realtime benchmark. Попередні спроби мали SIGBUS через вирівнювання embedded blob та невірний COPY-reloc `_Stdout` у link shim; виправлено .balign16 і вивід snprintf/write без FILE globals. Невдалі run085958/090217 збережено.

`artifacts/runs/20261015-090830/type103-analysis.json`: шість реальних committed packets,9 fragmentation sizes,36864samples, worst0.500LSB, обидва AAD,100000 trim arithmetic properties,50000 parser fuzz cases,18 rejections,guestexit0. Це синтетичний APAP transport із тестовим ключем, не новий iPhone сеанс і не native audio output.

Host: `logs/reference-audio-tests-v3.log`, `logs/reference-interposers.log` PASS, зокрема64 races/restart,32 stale SETUP cases, generation control/flush, NULL/missing/empty/selective teardown. Попередні два запускі загального runner не пройшли через пробіл у шляху та відсутню залежність coverart; виправлено runner path через тимчасовий symlink і експорт точних jpeg_safety sources, не тестову логіку.

## Точний профіль K3346 AirPlay

`artifacts/full-audio/port/k3346-profile.json`, `scripts/prepare_full_audio_port.py`. SHA256 libairplay: `34503a9f18799420005d7bd40c3cbf84634c8c9030bb0dcba79d519f0227e949`.

| Поле | K3346 NVIDIA | Reference MHI2Q |
|---|---:|---:|
| EnsureCreated | 0x2a164 | 0x2e9ac |
| SetPlistResponse | 0x2a7f8 | 0x2f040 |
| HTTP dispatcher | 0x2b9f0 | 0x302e0 |
| Server global | 0xab02c | 0xbabcc |
| Session clock slot | 0x13c8 | 0x13d0 |
| Session sockaddr family (static derivation) | 0x51 | 0x55 |

HTTP method/body offsets і stream type/audioType збігаються. Статичні anchors PASS, чужий hash відхиляється. Generated receiver source НЕ є готовим NVIDIA port: він усе ще має QSA абстракцію; native profile/session validation і NVIDIA sink залишаються необхідними.

## Маршрут саме нашої внутрішньої акустики

1. `scripts/inspect_rcc_ifs.py` розпакував обидві частини K3346 RCC ifs-root (233 та664 LZO1X blocks) з перевіркою imagefs additive checksum. Stage2:43,476,228bytes. Оригінал не змінений. Сам IFS має QNX duplicate namespace entries, тому витягнуто лише вибрані regular audio evidence, усі записи збережено в manifest.
2. Зчитаний із авто `rcc-audiotable-G22.bin` ПОБІТОВО збігається з gzip XML у firmware K3346. Це підтверджує таблицю; не є живим hash MIBRoot/DSP.
3. `MIBRoot` SHA256 `e12870cd748e59ea28e1d0cb2499312238d3f30539684d389ca031cebd5d4993`. Єдиний resource0x78 record: file0xc627dc. Internal `(src0x46e,sink1)`, external `(src0x46f,sink2)`. Кандидат двох полів internal — file0xc627e0/0xc627e4. Це локальна адреса K3346, а не0xce05a8 із MHI2Q. Машинний доказ `artifacts/rcc-route-analysis.json`; неправильний binary fingerprint відхиляється.
4. BGZ K3346 DSP розпакований у38records; code+дані в `artifacts/rcc-dsp-sections/`. Для аналізу локально зібрано GNU binutils2.44 tic6x-elf у /private/tmp. Правильна опція disassembly — `-EL`; без неї binary input помилково читається як big-endian. Це дослідницькі утиліти, не компоненти пакета.
5. Відновлені C674x xrefs: ReaderSettings0x875317e4, NVIDIA McASP selection0x87533544, PreDownMixer0x8755b1e4, MainMix0x8755b724. Reader constant2 at0x875317e0 йдe до кількох geometry fields. Не змінювати його глобально без простеження конкретного media reader і решти голосових шляхів. У NVIDIA вже інша McASP конфігурація, тому QC patch із референсу тут не застосовний автоматично.

**Висновок:** наявних даних вистачає для точного RE маршруту. Копіювання B&O/QC patch або зміна кодування на зовнішній amplifier не є реалізацією internal5.1. Потрібні media-specific reader geometry, PreDownMixer/MainMix input mapping, підтвердження INIC BW та відновлення RCC images; до завершення цього не створено flash script і не дозволено advertised6ch для авто.

## Відтворення локальних перевірок

- `bash scripts/build_full_scene.sh`; `python3 scripts/prepare_native_media.py --full-scene`; `AUDIO_PROBE_FULL_SCENE=1 python3 scripts/run_native_probe.py`.
- `bash scripts/build_native_type103.sh`; `python3 scripts/prepare_native_media.py --type103`; `AUDIO_PROBE_TYPE103=1 python3 scripts/run_native_probe.py`.
- Виконувати prepare тільки без активного власного/користувацького VM. Images isolated, snapshot-on; QEMU sources не редаговані.
- Build потребує локальних public QNX header declarations у /private/tmp/mhi2-openqnx-headers та існуючого ARM toolchain. Це не повний SDP SDK. Link shims НІКОЛИ не копіюються до guest/SD.
- `python3 scripts/analyze_rcc_route.py`; `python3 scripts/prepare_full_audio_port.py`; Python compile та Bash syntax PASS.

Немає vehicle writes/install, commits або push. Попередні бінарники/архіви збережені; нових production-ready заяв немає.


## Додаткова конкретизація DSP

Розпаковані787 TI.cinit records зі штатної області0x876113c8, із відповідністю ROM source VA → runtime destination. Файл: artifacts/rcc-dsp-sections/initialized/records.json; відтворює inspect_rcc_dsp.py. GP B14 ініціалізується у0x876237b0. NVIDIA McASP3 rx/tx конфіги:0x8760b968/0x8760b990, AFSRCTL0x412,RTDM0xff; QC0x8760bae8/0x8760bb10 має0x113/0x3. NVIDIA descriptors0x8760ba94/0x8760b9fc вже посилаються на свої TDM8 config і містять значення2 уword9. Семантику цього поля та reader geometry ще треба довести, перш ніж назвати конкретний byte patch. Це не повторення QC-плану.

Окремо зібрана PIC ARM/QNX бібліотека artifacts/native-type103/libmhi2_apac.so з актуального reference bridge. Вона незалежна від інших модулів; не включена у старий preview ZIP. Динамічне завантаження тепер перевірене у незалежному NVIDIA probe нижче; це ще не інтеграція з receiver.

## NVIDIA output component — новий перевірений етап

`prototype/nvidia_pcm_sink.{h,c}` використовує справжній NVIDIA API, без удаваних QSA byte counts. Mutex серіалізує write/close/reopen; generation відхиляє старі записи. Помилка native write лишає поточний chunk невизначеним, тому він не повторюється й не зараховується як прийнятий. Підтверджений API префікс означає прийняття/буферизацію, не відтворення на динаміку. Fault зупиняє подальші записи до явного reset. Число20ms передається кожному native виклику; жорсткий загальний realtime deadline цим не доведений. Route eligibility зараз явно дозволена тільки в emulator MPL1 probe.

`bash scripts/build_native_nvidia_sink.sh`: host lifecycle/fault/cleanup та concurrent write/flush tests PASS. QNX run `artifacts/runs/20261015-131215`: справжня динамічна APAC library →6packets/6144frames →NV MPL1, reopen, stale generation rejection, close, guest exit0. `analyze_apac_nvidia.py`: десять256-frame windows within1LSB PASS. Цей AAC-independent APAC fixture має ненульові лише L/R, тому він сам не доводить шість незалежних каналів.

Окремі onehot сигнали у тому самому run: шість contiguous exact plateaus по28673–32257frames, off-channel peak0,6/6 PASS (`onehot-analysis.json`). Канал1 також має2560 нееталонних frames між першим/останнім точним зразком: загальна безперервність потоку НЕ доведена. Попередній run130751 з12ms sleeps не пройшов plateau gate; результат збережений. Новий probe подає64blocks без sleeps, а settling/silence виносить за marker. Це виправляє експеримент, але НЕ доводить, що close/reopen очищає весь downstream ring. Повний FLUSHBUFFERED не вважати завершеним.

## AAC: платформна відмінність

У точного K3346 dio_manager немає libNmeSDK у DT_NEEDED; у перевірених stock emulator directories eso/lib,armle/lib,armle/usr/lib немає Nme/Cinemo entries. Reference Cinemo bridge не можна оголосити придатним без заміни backend. Реалізований автономний decoder-only FDK backend (upstream commitc41c3c7516ea8d0d78a53b96b30bed3a29745cc6), `scripts/build_fdk_decoder.py`, `prototype/fdk_aac_bridge.c`. Таблиця `spa_aac_codec_api` має ABI референсу, власні decoder instances, bounded nested cookie parsing і точний контракт index23 AAC-LC48k/stereo; невідома geometry відхиляється.

Host raw test PASS:25packets,25600frames, окремі697/1209Hz канали. Bridge test PASS:4043malformed cookies, без споживання AU при замалому буфері, точний reset/zero-length reset, fault latch до reset. QNX canary run `artifacts/runs/20261015-132548` PASS: обидві справжні shared libraries динамічно завантажені; два direct runs та два codec-table runs (із reset) дають однаковий native FNV31616389, exit0. `aac-analysis.json` явно зберігає відмінність від host FNV1460631062; причина/повна точність між архітектурами ще не доведена. Peaks обох платформ10550/6295. Це **не** доказ live APAC↔AAC switching, renderer integration чи gap-free playback.

Три попередні prepare відхилені через restore capacity до запису. Незмінені decoder sources зібрані з-Os, public exports та section GC:512KiB замість856KiB. Окремий stock-language emulator clone `artifacts/native-media-aac`; фінальний IFS29622272 вкладається у29652992bytes. `prepare_native_media.py --aac`; `AUDIO_PROBE_AAC=1 python3 scripts/run_native_probe.py`; `analyze_native_aac.py RUN`. Увесь upstream cpp source+NOTICE збережений в artifacts/fdk-decoder-source-c41c3c7.tar; NOTICE поруч з library. Нового пакета M.I.B. немає.

Додаткова перевірка RCC: `logs/rcc-reader-geometry.txt` показує, що MVK2 при0x875317e0 використовується кількома48-byte reader entries. У `logs/rcc-mcasp-reader-candidate.txt` instruction0x87532a2c вже MVK6,a5, тож site3 із reference plan не переноситься за тією самою адресою. Жоден із цих оглядів ще не доводить конкретного media-specific patch.

Остання AAC-конфігурація: **run20261015-133323 PASS**, автоматичний FDK limiter явно вимкнений (його default додає15ms lookahead, якого clock receiver не враховує). Чотири native direct/API/reset passes мають FNV3189702696; host без limiter —2466064558, тому міжархітектурна побітова тотожність і надалі НЕ доведена. Усі host bridge перевірки повторно PASS. Поточні FDK/bridge hashes збігаються з manifest реально запущеного media; IFS29622784bytes у межах capacity. `SOURCE_DATE_EPOCH` зафіксований timestamp upstream commit, щоби __TIME__ в library metadata не змінював fingerprint при кожному build. Нативний analyzer вимагає marker вимкненого limiter; старі runs залишені як історичні.
