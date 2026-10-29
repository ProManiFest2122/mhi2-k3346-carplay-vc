# Як тракт MPL1 маршрутизується та (не)мутиться: статичні докази (2026-10-15)

Джерела: бінарники й конфіги з повернення (`handoff-windows/.../evidence`), логи сесії `car-tests/onehot6-session-20261015/`. Лише читання; «встановлено» = видно в рядках/логах, «гіпотеза» = висновок без доказу.

## Встановлено
- **mm-renderer** (медіаплеєр USB/SD/BT) пише у MPL1: `mmr-nvidia-routing.so`, `writer_prio = 17` (`/etc/mm/mm-renderer.conf`); у sloginfo `OChannel[0].id=MdevOutCA4M`, стерео, 44.1 кГц (з логів сесії 2026-10-15).
- **mediarouter** імпортує `nvaudio_control_open/close/set_route`, `NvMS_ServerCreateSource/DestroySource/SetRoute` і має `CAudioRoutingService`/`CAudioRoutingServiceJob_SetAudioRoutes`:
  саме він задає матрицю маршрутів (`libnvaudio` -> `NV_IOCTL_SET_ROUTE_CONFIG`, у deva-логах «Will try to route output TDM1_OUT_MPL to input: MPL5» перед CarPlay-відтворенням).
- **deva-nvaudio** не має логіки мюту/дакінгу: це PCM-мультиплексор (`NvAudioMuxStreamsTDM1/2`, `FadeIn/FadeOut` лише на старті/кінці буфера). `nvaudio_play` дійшов до цього шару («New buffer from MPL1», `DTCP enabled!` у 6ch).
- **Мют/вибір джерела не в MMX-шарі, який ми бачимо**: AudioTable AU_G22 v233 (RCC) описує джерела `LS_ENT_AMP_*` і віртуальні функції `LS_FCT_AMP_MUTE_USER/STANDBY/THEFTPROTECTION/DOWNLOADMENU`, `LS_FCT_AMP_RELEASE_ALL`, прапорці `flag-allvolrst-on-start/-volrst-on-stop`, `flag-connectable`.
  Типова схема: джерело активується запитом аудіо-менеджменту (RCC) -> розмут/мікс у DSP-ланцюгу (`hZone0_*`).
- CarPlay-шлях у авто: `AudioStreamPrepare` -> «Modes changed: mainAudio controller» -> route `TDM1_OUT_MPL <- MPL5`. `nvaudio_play` не проходить ні запит джерела, ні `SET_ROUTE_CONFIG`; для MPL5 записи завершились `status = 4` (маршрут не заданий).

## Гіпотези (не доведені)
1. Тишу на MPL1 пояснює відсутність активного AM-джерела: у стані «зовнішній програвач вибрано, не грає» RCC тримає мют/нульове джерело. Перевіряється сесією 2 (S0/S1/S2).
2. `ann*`-пристрої (системні/навігаційні) звучать незалежно від медіа-джерела, бо мають власні запити (nav йде в `ANN3` за логами). Перевіряється A2/A3/B3/B4.
3. Кількість каналів (6) сама не є причиною тиші: тоді стерео на MPL1 у S0 теж мовчатиме (A1). Перевіряється A1 проти B1.

## Не знайдено статично
Точна умова мюту в RCC (код AM, мапа `LS_ENT_AMP_*` -> вхід DSP, який підсилювач 120/121/122 стоїть). Для цього потрібні RCC-журнали під час відтворення (сесія 2 збирає їх best-effort) або розбір RCC-коду аудіо-менеджменту.
