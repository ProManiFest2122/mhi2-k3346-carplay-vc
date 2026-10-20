# 10 – Jak odtworzyć build od zera (zapis stanu na 2026-10-20)

Po co ten dokument: artefakty leżą w `port_k3346\...\output\` i `work\`, ale **`work\` nie jest
pod kontrolą wersji i nie jest w backupie**. Gdyby przepadł, bez tego zapisu nie dałoby się
odtworzyć paczki. Tu są dokładne komendy, zależności i oczekiwane sumy kontrolne.

Pisane dla: kogoś (także dla mnie w przyszłej sesji), kto musi zbudować to samo jeszcze raz.

---

## Czego NIE da się odtworzyć z samego repo

| Co | Skąd to jest | Uwaga |
|---|---|---|
| `work\lsd.jar` (132 MB) | `work\lsd.jxe` przez `repos\jxe2jar` | Zależność kompilacji Javy. **Nie jest dystrybuowana** – pochodzi z firmware'u tego auta |
| `work\lsd.jxe` (58 131 096 B) | backup M.I.B. / `/ifs/lsd.jxe` z auta | `sha256 e43d80e7eeb803d6a7db29908562b9545e7b17138d4660ce3a6efafa974b99ce` |
| `work\sd_odczyt_1\` | collector v1.2 uruchomiony w aucie (Etap 1) | Zawiera **dane osobowe** (VIN, kontakty, GPS w `iap2.log`) – nie publikować |
| `work\mmx_app`, `work\pkg_app70` | wypakowane z backupu / paczki K3346 | Przez `tools\qnx6_extract.py`, `tools\qnx_ifs_extract.py` |

Źródła portu: `edzenis/mib2high-carplay-altscreen-rgi @ bfea86859a5bdaed3c29a16f03fc7ba87ef4a19e`
(zapisane w `port_k3346\UPSTREAM.txt`), z naszymi zmianami dla K3346.

---

## Krok 1 – obraz kompilatora QNX

```bash
cd repos/qnx65-armv7-toolchain
docker build --platform=linux/amd64 --build-arg BASE=base-4.9 -t qnx65-armv7-toolchain:4.9 .
```

- Buduje GCC **4.9.4** dla `arm-unknown-nto-qnx6.5.0eabi` ze źródeł (~20–40 min).
- W obrazie jest shim `qcc` (`/opt/tools/qcc/bin/qcc`) tłumaczący `qcc` → cross-GCC.
- Sprawdzenie: `docker run --rm --platform=linux/amd64 qnx65-armv7-toolchain:4.9 sh -c 'command -v qcc'`
- **Nie używać `bash -lc`** – login shell resetuje `PATH` przez `/etc/profile` i `qcc` znika.

## Krok 2 – trzy binarki natywne

```bash
bash tools/qnx_build.sh build.sh                             output/libmib2high-carplay-rgi.so
bash tools/qnx_build.sh renderer/build_renderer.sh           renderer/build/vc_render
bash tools/qnx_build.sh diagnostics/build_dsitracer_qnx_v8l.sh diagnostics/v8l-dsitracer-capture
```

Etap 4 (biblioteka RGI tylko dla `mm-ipod`, bez AltScreen, `docs\15`):

```bash
bash tools/qnx_build.sh build_rgi_ipod.sh output/libmib2high-carplay-rgi-ipod.so
```

Paczka Etapu 4 trafia do `work\sd_etap4\` (`make_sd_package.sh`, zmienna `SD_OUT` zmienia katalog).

`tools\qnx_build.sh` jest potrzebny, a nie goły `docker run`, z dwóch powodów:

1. **QNX binutils (`nm`, `ld`) to 32-bitowe binarki x86 bez obsługi dużych numerów i-węzłów.**
   Bind-mount Docker Desktop na Windows podaje i-węzły nie mieszczące się w 32 bitach, więc
   `stat()` w `nm` zwraca EOVERFLOW i linkowanie pada:
   `nm: Warning: could not locate '....o'. reason: Value too large for defined data type`.
   Kompilacja przechodzi, pada dopiero link. Wrapper kopiuje źródła do systemu plików kontenera,
   buduje tam i kopiuje artefakty z powrotem.
2. Skrypty edzenisa oczekują `$QNX_RUNTIME/activate.sh`, czego obraz nie ma – dostarcza to
   `work\qnx_runtime\activate.sh` (shim, nie podmienia kompilatora, tylko potwierdza, że `qcc`
   i cross-GCC są w `PATH`, i ustawia `QCC_TARGET`).

W `tools\qnx_build.sh` jest **zahardkodowana ścieżka projektu** (`PROJ_WIN`) – przy przeniesieniu
projektu trzeba ją poprawić.

## Krok 3 – JAR (Java)

```bash
MSYS_NO_PATHCONV=1 docker run --rm --platform=linux/amd64 \
  -v "<projekt>/port_k3346/altscreen:/src" \
  -v "<projekt>/work:/work" \
  -v "<projekt>/repos/jxe2jar/jvms:/jvms:ro" \
  -e JAVA_HOME=/jvms/zulu8.78.0.19-ca-jdk8.0.412-linux_x64 \
  -e K2161_BASE_JAR=/work/lsd.jar \
  qnx65-armv7-toolchain:4.9 sh /src/java/build_java_release.sh
```

- JDK 8 bierzemy **linuksowy** z `repos\jxe2jar\jvms\` i uruchamiamy w kontenerze (jest tam też
  `win_x64`, ale skrypt budujący to POSIX sh).
- Oczekiwane: `JAVA_CLASS_FORMAT=PASS 29 classes (major 48)` – bytecode 1.4, wymagany przez JVM w MIB2.
- Zmienna nazywa się `K2161_BASE_JAR` (nazwa od edzenisa), ale podajemy **nasz** `lsd.jar` z K3346.

## Krok 4 – paczka na kartę SD

```bash
bash tools/make_sd_package.sh        # -> work/sd_etap3/ + SHA256SUMS.txt
```

Skrypt sam sprawdza: brak CRLF w plikach tekstowych, binarki są ARM, wszystkie artefakty obecne.
Instalator trafia do paczki jako **`mod/command.sh`**, bo M.I.B. 3.6.0 uruchamia wyłącznie ten plik.

## Krok 5 – weryfikacja

```bash
# offsety hooków przeciw PRAWDZIWEMU driverowi z auta – musi być 17/17 ALL PASS
python tools/verify_port_offsets.py port_k3346/altscreen/native \
  "work/sd_odczyt_1/MIB2_SOFTWARE_COLLECTOR_v1.2_PID_8523907/files/mnt/app/armle/lib/dll/ipod-drvr-iap2.so"

# symulacja instalatora – musi być 62 OK, 0 FAIL
# (dodatkowo warto pod mksh: apt-get install mksh; ln -sf /bin/mksh /bin/sh – w kontenerze --rm)
MSYS_NO_PATHCONV=1 docker run --rm --platform=linux/amd64 \
  -v "<projekt>/work:/work" \
  -v "<projekt>/tools:/tools:ro" \
  qnx65-armv7-toolchain:4.9 bash /tools/sim_installer.sh

# sumy paczki
cd work/sd_etap3 && sha256sum -c SHA256SUMS.txt
```

**Symulację trzeba uruchamiać w kontenerze, nigdy na Windows** – awk na Windows zjada `\r`,
a pliki HMI w aucie mają mieszane końce linii (CRLF + LF), więc wynik na Windows jest nieprawdziwy
(zamiast 3 zmienionych linii pokazuje przepisanie całego pliku).

---

## Krok 6 – karta ratunkowa (`docs\12`)

```bash
# M.I.B. 3.6.0 = ta sama wersja co w aucie; wydanie nie ma plików, to zawartość tagu
mkdir -p work/downloads/mib_3.6.0
curl -sL -o work/downloads/mib_3.6.0/mib_V3.6.0.zip \
  https://api.github.com/repos/Mr-MIBonk/M.I.B._More-Incredible-Bash/zipball/V3.6.0
#  sha256 bff859bb364f7188100dded75aac9154e0df1bfe67687e39146237d99949e564 (pobrane 2026-10-20)
python -I -c "import zipfile; zipfile.ZipFile('work/downloads/mib_3.6.0/mib_V3.6.0.zip').extractall('work/downloads/mib_3.6.0/x')"

bash tools/make_rescue_card.sh           # -> work/sd_ratunek/

# symulacja – 45 OK, 0 FAIL; tylko w jednorazowym kontenerze (tworzy /net/mmx, /mnt/app, /sd,
# dopisuje uzytkownika do /etc/passwd i uruchamia przez setpriv – w obrazie nie ma useradd, su odmawia)
MSYS_NO_PATHCONV=1 docker run --rm --platform=linux/amd64 \
  -v "<projekt>/work:/work" \
  -v "<projekt>/tools:/tools:ro" \
  qnx65-armv7-toolchain:4.9 bash -c 'apt-get update -qq && apt-get install -y -qq mksh &&
    ln -sf /bin/mksh /bin/ksh && ln -sf /bin/mksh /bin/sh && bash /tools/sim_rescue.sh'
```

**Rozpakowywać przez Pythona, nie przez `git clone`** – `core.autocrlf=true` na tym komputerze
zamieniłby końce linii w skryptach M.I.B. na CRLF. `make_rescue_card.sh` i tak to wykryje
(SHA1 `finalScript.sh` musi się zgadzać z `metainfo2.txt`, a skrypty nie mogą mieć CRLF).

## Oczekiwane sumy kontrolne artefaktów

| Artefakt | Rozmiar | SHA-256 |
|---|---|---|
| `libmib2high-carplay-rgi.so` | 306 904 | `058413ddc3f732ddf65488e9d273535b2b1507a572dccff42ac7dc157220e479` |
| `mib2high-carplay-rgi.jar` | 68 962 | `606e11a0b4883c73ae41bb691255550c7eef489558799a052700dc365b6f1529` |
| `vc_render` | 29 423 | `fd5085d811efed1d2676f0f26fab342708762591c043835ec5c7ec66afe3b144` |
| `v8l-dsitracer-capture` | 12 612 | `07076bf196baa180586a589b5a1d621a355bd2fa24d512486bbcc82306875632` |
| `work\lsd.jar` (zależność) | 132 029 447 | `f1f1df905a543a981eb1e5960b81a95b5fec39148fe5a5ddb65ce62979bad6a3` |

**`v8l-dsitracer-capture` jest bit w bit identyczny z binarką z oficjalnego wydania AltScreen v1.0
edzenisa.** To najmocniejszy dowód poprawności środowiska: ten sam plik z tych samych źródeł
oznacza, że nasz toolchain to ten sam kompilator, którego użył autor. Pozostałe różnią się,
bo `.so` i `.jar` zawierają nasze zmiany dla K3346, a `vc_render` ma debug-info (`-g`).

Reszta artefaktów **nie musi** mieć tych sum przy ponownym budowaniu, jeśli zmieniono źródła –
wtedy liczy się `verify_current_native.py` (QNX ARM ELF + eksporty) i symulacja.

---

## Co jest nasze, a co edzenisa

Zmiany wprowadzone w porcie K3346 względem commita `bfea8685`:

| Zmiana | Plik |
|---|---|
| Offsety i adresy dla K3346 | `altscreen\native\` (observer, ident_hook, message_table, send_hook) |
| Rozdzielenie `updateManeuverDescriptorAndExitView` → `updateManeuverDescriptor` + `updateExitView` (w K3346 połączona metoda to atrapa) | `altscreen\java\build\src\routeguidance\BAPBridge.java` |
| **Brama wersji** (`HMI_Version` + rozmiary `ipod-drvr-iap2.so` i `lsd.jxe`); u edzenisa brak jakichkolwiek bramek | `altscreen\deployment\custom.sh` |
| `collect()` zbiera `dmdt-gc.txt` (stan kontekstów, tylko odczyt) | `altscreen\deployment\custom.sh` |
| Marker **`COLLECT_LOGS`** – wymusza zbieranie logów niezależnie od stanu instalacji | `altscreen\deployment\custom.sh` |
| Alternatywny profil geometrii B9 1440×540 | `altscreen\config\mib2high-lvds-profile.b9-1440x540.conf` |

Zasada, której trzymamy się konsekwentnie: **zmieniamy źródła i przebudowujemy, nigdy nie
łatamy binarek ani bytecode'u.**
