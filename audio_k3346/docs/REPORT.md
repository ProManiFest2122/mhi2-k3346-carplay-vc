# K3346: перспектива 5.1, Spatial Audio та UpMix

Дослідження 14.10.2026, початок 17:03:26 UTC. Ціль: MHI2_ER_AUG22_K3346 / MU1438, Audi A4 B9, внутрішній підсилювач, додані центр і саб. Код автомобіля, його прошивку та параметрію не змінено. Експерименти виконані на Mac і на окремій копії медіа нашого емулятора; eMMC підключалася з `snapshot=on`.

## Висновок

Перенесення окремих аудіокомпонентів має перспективу. У штатному NVIDIA тракті K3346 доведено шість незалежних S16 каналів MPL1 і примусово маршрутизованого MPL5 до TDM. Реалізовано й перевірено локальний потоковий UpMix-адаптер, виконано APAC5.1 декодування на ARM/QNX. Готового CarPlay порту поки немає: штатний handshake/focus, real-time запас Tegra та шість фізичних виходів RCC не доведені.

Готовий пакет автора не встановлювати: він призначений для MHI2Q, а його аудіокалібрування і захист від перевантаження прив'язані до B&O. Для нашого блока потрібна власна інтеграція `libnvaudio`, штатних профілів та внутрішнього підсилювача.

## Поточна параметрія

Джерело — наданий власником оновлений файл `~/Downloads/MIB2_5F_sound_datasets_technical_findings_1.md`. Hex перевірено незалежно; це перевірка наданих байтів, а не нове зчитування з авто.

| Датасет | Розмір | Версія | CRC | Перевірка |
|---|---:|---|---|---|
| Установлений 0x3B00 | 55 | 12 | FA30 | збіг |
| Установлений 0x3000 v16 | 1330 | M5 | 4454 | збіг |
| Заводський 0x3B00 | 55 | 12 | 2180 | збіг |
| Відновлений заводський 0x3000 | 1330 | M5 | 692A | збіг |

У v16 `Signalflow=0x31`. У 0x3B00 профілі 1/2, регулятор саба прихований. CRC — CCITT-FALSE, init FFFF, останні два байти виключені. Бінарні копії й SHA-256: `artifacts/datasets/verified.json`.

Центр уже має два HPF2 450 Гц і LPF2 7 кГц; саб — LPF2 120 Гц, два HPF2 50 Гц, Peak 67 Гц +3 дБ і власний gain. Переносити B&O LFE Assist без урахування цієї обробки не можна. Шість налаштованих виходів датасету не означають автоматично шість незалежних каналів входу CarPlay.

## Перевірено виконанням

| Перевірка | Результат | Межа доказу |
|---|---|---|
| APAC `make -j4 check` | усі 17 тестових програм пройшли | committed fixtures; не нова iPhone-сесія |
| `scripts/test_hook_audio.sh` | пройшов, включно з APAP/type-103, reorder, flush, feedback, guard | host; без NVIDIA sink і RCC |
| Python scene reference | 54/54 | математичні й синтетичні сценарії |
| C scene math/engine проти нових Python fixtures | усі перевірки пройшли; block-level mask −131.3 dB RMS | host; не швидкодія Tegra |
| Власні сигнали | 96 рендерів, 549 перевірок, 12 strict block-invariance відмов | усі відмови обмежені LFE Assist |
| Емулятор `check_audio.py` | 3 тести пройшли | черги/PCM endpoint, не повний медіатракт |
| Ізольований QEMU hardware regression | PASS | DMA/SMMU/IRQ/pause/cyclic, не RCC DSP |
| Штатний K3346 `libnvaudio` + `deva-nvaudio` | MPL1–MPL6: 512 frames, 8 ch, 4 bytes, 48000 Hz; `set_pcm_info(48000,2,6)` повертає 0 | прийняття конфігурації, не фізична незалежність каналів |
| Негативний контроль | ANN1/TEL — mono; 6 ch повертає 7 | голосовий тракт не розширюється цим налаштуванням |

Власні сигнали: тиша, моно голосоподібний сигнал, інструмент ліворуч, протифаза, бас 50 Гц, стереосцена, паузи/зупинка та майже повна шкала. Перевіряли блоки 100/768/1024, OFF, spatial, mask і mask+LFE. OFF зберіг L/R побітово; додаткові канали нульові. Тиша точна; у перевірених сигналах немає S16 saturation. SCENE має 1024 frames / 21.333 ms затримки; C у стерео-UpMix навмисно нульовий.

LFE відрізняється максимум на 3 S16 LSB між блоками; L/R/C/Ls/Rs збігаються побітово. Причина, видима в коді: renderer спочатку просуває весь блок і mask-контролі, а потім формує LFE з останніх bass/drum posteriors. Контроль не прив'язаний до кожної позиції вихідного FIFO. Це не доказ чутної проблеми, але strict streaming gate не проходить. Відмови збережені, не перетворені на PASS.

Host bench: engine mean 54.6 µs / 768 frames, p99 151.8 µs; mask mean 111.0 µs / 512 frames, p99 354.7 µs. Ці числа характеризують цей Mac, а не Tegra 3. CPU margin під навігацією, CarPlay і HMI не виміряний.

## Що показав рідний тракт в емуляторі

Окрема QEMU-копія пасивно записує TX DMA після штатного mux. Патч лише захоплює байти; не змінює DMA, черги чи маршрути. Для capability/one-hot probe додано два окремі виконувані файли до копії IFS; SHA всіх оригінальних ресурсів збережені.

Оновлення після першого звіту: для S16 payload потрібні SAD allocation FC і sample-size 16. Поля capability «8 каналів / 4 байти» не визначають stride записаного payload. Розібрано оригінальний S16 mux: шість вхідних shorts на frame перетворюються на 32-байтовий TDM frame; канал 3 має рознесені байти. Прототип capture тепер записує index із часовими мітками, byte offsets і phase markers.

Запуск `artifacts/runs/20261014-174801`: усі шість MPL1 one-hot фаз незалежні, off-channel peak=0, 2564–3588 точних plateau frames на фазу. `scripts/analyze_native_tdm.py` завершився PASS. Це доказ нативного транспорту до TDM в емуляторі, не фізичних виходів RCC.

IFS завантажується також із eMMC restore. Обидві копії синхронізовано лише на окремому дослідному NOR і клонованому eMMC; початкові ресурси збережені. Пробні LD_PRELOAD перезапуски mediarouter не пройшли (бібліотечне оточення / bundle-loader initialization), їх не трактуємо як позитивні. У наступному ізольованому тесті зупинено `servicemgrmibhigh` та mediarouter; звільнений штатний controller прийняв маршрут MPL5→TDM1_OUT_MPL до запису PCM, всі 6×12 викликів write повернули 0. Запуск `artifacts/runs/20261014-175807` завершився штатно; аналіз його TDM захоплення пройшов: 6/6 незалежних фаз, off-channel peak=0.

Зібрано `native-scene-probe` для ARMv7/NEON: portable SCENE renderer плюс штатний libnvaudio sink, stereo→L/R/Ls/Rs, C/LFE нульові. Запуск `artifacts/runs/20261014-185258` пройшов: 97536 output frames, 191 writes без помилок, NaN/clipping=0, reset silence=0; суми абсолютних значень відрізняються від host до 11 integer counts. Це агрегатне порівняння, не доказ по кожному sample. Link stubs потрібні лише збирачу, в IFS їх не додаємо; виконання має використовувати штатні QNX бібліотеки. LFE Assist вимкнений.

Статично розпаковано 38 BGZ-ресурсів саме RCC DSP K3346. Є `hPreDownMixer`, `hZone0_MainMix`, конфігурації NVIDIA/QC McASP. Це підтверджує наявність наступного шару; емулятор не виконує справжній Jacinto DSP, тому не перевіряє нашу параметрію акустично.

## Рішення для перенесення

1. **Спершу транспорт без UpMix.** Шість one-hot каналів мають незалежно пройти штатний MPL5/CarPlay → TDM → RCC маршрут. Перевірити порядок, SAD, sample width, gain і повернення до стерео. MPL1 з дослідного запуску не замінює перевірку MPL5.
2. **Потім обмежений UpMix.** Portable scene DSP, власний NVIDIA adapter, LFE Assist вимкнений, калібрування під нашу параметрію; штатні телефон/Siri/підказки окремо. Перевірити OFF, mute, focus, reconnect, sleep і всі режими All/Front.
3. **Spatial Audio окремо.** Portable APAC decoder та type-103 framing/auth/cookie parser; перевірка AirPlay 210.81 точок підключення саме MU1438. APAC 5.1 після доведеного six-channel sink. 5.1.2/height не включати у перший порт; автомобіль має шість виходів, а layout index 66 в автора дослідний.
4. **HMI останнім.** Новий UpMix runtime control окремо від двох CarEQ-профілів. Не розширювати 55/1330-byte датасети та не замінювати український `lang_data.zip` чужим архівом.
5. **Адекватність звуку — в авто.** Після офлайн transport gates потрібні вимірювання каналів/баса, level-matched слухання й loaded Tegra P99/underrun trace. Емулятор цього не заміняє.

## Відтворення та джерела

Дослідні команди й скрипти — `scripts/`, результати — `logs/`, WAV/JSON/ELF/пасивний QEMU-патч — `artifacts/`. Запуски не є інсталятором автомобіля. Штатні медіа та shared QEMU source не змінені; commit/push не виконувалися.

- [mib2q-carplay-rgi, pinned 10fec320](https://github.com/luka-dev/mib2q-carplay-rgi/tree/10fec32079477b31e03683aee453eb4faf21442b)
- [MHI2 MU1439 port study](https://github.com/luka-dev/mib2q-carplay-rgi/blob/10fec32079477b31e03683aee453eb4faf21442b/docs/reference/MHI2_MU1439_PORT.md): близька US-платформа, не доказ для ER MU1438.
- [Internal amp study](https://github.com/luka-dev/mib2q-carplay-rgi/blob/10fec32079477b31e03683aee453eb4faf21442b/docs/audio/INTERNAL_AMP_PATH.md): переважно MHI2Q/QC, адреси не переносити на наш RCC.
- [Dataset templates, pinned f23bfbf](https://github.com/NumberOneBot/mqb-mib-sound-datasets/tree/f23bfbfb6c6783bc413fd7f1f7565182ee3f9303): MQB-джерело структури; наші CRC/довжини перевірені окремо.

## Продовження: потоковий прототип

`prototype/mhi2_scene_adapter.{h,c}`: обмежений S16/48k stereo→six-channel adapter, без виділення пам'яті після init, FIFO4096 frames, явна затримка1024 frames. C/LFE нульові. OFF розширює stereo побітово; mute/reset очищають хвіст; невірний розмір відхиляється до споживання input. Host fixture49152frames, шість packet sizes1/100/257/512/768/1024: побітова тотожність, усі lifecycle/size gates PASS (`logs/test-scene-adapter.log`).

QNX запуск `20261014-185639`: той самий adapter передав98304frames/192writes зі статусом0, invalid/clipping=0, reset silence=0. Агрегатні виходи ARM близькі до host, але wall-clock realtime на Tegra не виміряний.

`prototype/nvaudio_upmix_fixture.c`: пробне перехоплення публічного PCM API для одного серіалізованого синтетичного MPL1 клієнта. Упізнає stereo48k/S16, конфігурує native6channels, подає FC16 metadata і результат адаптера. Це НЕ production hook: конкурентний доступ, focus, retry при частковій передачі, більші пакети та реальний AirPlay lifecycle ще не реалізовані.

Запуск `20261014-185919` із12ms sleeps: write192/192success, але strict captured512frame comparisons FAIL. Невдача збережена. Запуск `20261014-190515` без штучних sleeps і з timeout250ms: pre-driver output aggregates збігаються з ARM adapter; metadata261120=0x3FC00;192writes без помилок. Шість вибраних TDM512frame сегментів збігаються з host у межах1LSB, серед них є побітово точні. Це підтверджує pipeline на цих ділянках, НЕ весь потік: DMA gaps/fade/queued prior traffic залишаються. Аналіз не прирівнює rejected segments до PASS (`upmix-analysis.json` містить усі examined segments).

Статичний нюанс саме K3346 `CAudioDriverIF::configure` у libairplay210.81: нижній `nvaudio_set_pcm_info` викликається лише при зміні sample-rate (0x449DC–0x44A0C). Тому hook, який активується тільки через цей виклик, може взагалі не активуватися у штатній48k CarPlay сесії. Потрібна інтеграція native AudioStream/configure/SAD, а не перенесення fixture на автомобіль. Дизасемблювання: `logs/airplay-native-configure-disasm.txt`.

## ARM/QNX APAC5.1

Запуск `artifacts/runs/20261014-191147`: native APAC probe декодував6 committed real packets у6channels,36864samples. invalid=0; max relative error235ppb=2.35e-7 проти PCM Apple/Pioneer fixture (gate5000ppb=5e-6). Flush status0/frames0; guest exit0. `apac-analysis.json` зберігає машинний verdict. Свіжі iPhone пакети не отримані. Це підтверджує codec execution у штатному QNX, не type103 negotiation або output RCC.

Джерела з pinned commit скопійовано без змін до `upstream/` із LICENSE/NOTICE; build scripts більше не залежать від тимчасового clone для SCENE/APAC. `scripts/check_prototype.sh` відтворює host adapter gates і PCM reference. Додатково перевірено Python/bash syntax та незмінний SHA початковогоNOR.

Сумарний статус: **можна переносити й тестувати власним NVIDIA портом**, але **встановлюваного пакета для авто ще немає**. UpMix першого прототипу формує L/R/Ls/Rs; центр іLFE навмисно0, штатна параметрія продовжує визначати наступний DSP тракт. APAC має6каналів у codec output, проте не підключений до liveAirPlay. LFE Assist, height/5.1.2 та B&O calibration не включені.

## Продовження за живими даними15.10

Аналіз звужено до аудіо за вказівкою власника. Live core binaries збігаються з попереднім stock;48k stereo music та16k mono prompts підтверджені. Форматні transitions/error latch і окрема native AirPlay same-rate configure seam додані у prototype. Детальні докази, межі й нові тести: LIVE_AUDIO_IMPLEMENTATION_20261015.md. Це дослідне продовження, не готовий installer для авто.


## M.I.B. preview delivery — 2026-10-15

Current delivery and verification: [MIB_AUDIO_DELIVERY_20261015.md](MIB_AUDIO_DELIVERY_20261015.md). Final archive artifacts/mib-audio-final-preview.zip; CHECK default, Off first boot. Installer simulation6/6, Java persistence and host DSP PASS; native QNX levels0–4 PASS run083738. No vehicle install or physical audio validation. APAC type103 is not integrated.
