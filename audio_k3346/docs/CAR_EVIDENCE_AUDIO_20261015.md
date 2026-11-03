# Аудіо-докази з повернення Windows (авто), 2026-10-15

Джерело: `handoff-windows/MHI2_RETURN_20261015-20261015-073857/` (зняте з авто лише читанням: sloginfo сценаріїв, бінарники з SHA-256, конфіги, `rcc-audiotable-G22.xml`).
До цього аудіопроєкт брав звідти хеші core binaries/конфіги (`build_mib_audio*.py`, тести інсталятора), але не читав саме аудіо-докази. Перевірено 2026-10-15:

- **Ідентичність бінарників.** `libairplay.so` (sha256 34503a9f...) і `libnvaudio.so` (aeb91534...) з повернення збігаються з тими, що в емуляторі (`artifacts/stock`); `prepare_full_audio_port.py` перевіряє той самий відбиток libairplay.
  `dio_manager` bd42c9ee..., `deva-nvaudio` 1733e7e3... (deva запущено як `-2 -1 -p 21 -i 26`).
- **Реальний пристрій CarPlay-музики в авто = MPL5.** У sloginfo сценаріїв: `NV_IOCTL_SET_ROUTE_CONFIG: Will try to route output TDM1_OUT_MPL to input: MPL5` (10 подій у S2-S7, X07, X08, FINAL), поруч
  `Main audio setting up PCM/48000/16/2 for media`, `Buffering 50 [ms]`. MPL1 з'являється як маршрут за замовчуванням на старті (`already routed to input: MPL1`, 5 подій). Навігація йде в `ANN3`
  (`TDM1_OUT_ANN1 <- ANN3`). Це закриває питання «MPL5 кандидат» як спостереження з авто (читання логів), але не доводить прохід 6ch. Емуляторні тести використовують MPL1 (`SPA_NV_STOCK_DEVICE`); пакет-чернетка за замовчуванням MPL5.
- **Стоковий CarPlay-звук у авто стереофонічний**: PCM/48000/16/2 (feature F10 у `feature_matrix.csv`).
- **AudioTable AU_G22 v233** (`live-comms/rcc-audiotable-G22.xml`): внутрішні підсилювачі `120 AU_2CH`, `121 AU_4CH`, `122 AU_6CH` (id 120/121: subwoofer=0, speechOptimization=0; 122: типові значення з subwoofer=1);
  зовнішні MOST: 140/141/151/160/161/181/182. Блоки settings/constraints AU_4CH і AU_6CH збігаються (гучність/loudness), тобто таблиця аудіо-менеджменту сама не змінює маршрутизацію каналів. Який підсилювач стоїть у цьому авто, у поверненні НЕ визначено
  (`live-comms/rcc-irc/SETTINGS`, 97 Б, формат не розібрано).
- Фізичне відображення RCC/каналів у поверненні не знімалось (NOT_TESTED): є лише `/dev/nvaudio-tdm`, `/dev/shmem/NVshm-tdm-*`.
