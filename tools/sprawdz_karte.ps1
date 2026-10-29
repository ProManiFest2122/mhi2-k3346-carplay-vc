<#
.SYNOPSIS
  Sprawdza karte SD przygotowana do Etapu 3. TYLKO ODCZYT - niczego na karcie nie zapisuje.

.DESCRIPTION
  Zrodlem prawdy jest paczka na KOMPUTERZE (work\sd_etap3 albo work\sd_ratunek):
  kazdy plik z jej SHA256SUMS.txt musi byc na karcie i miec te sama sume SHA-256.
  Do tego sprawdzenia typowych pomylek, ktorych nie widac golym okiem:
  folder skopiowany zamiast zawartosci, zly system plikow, uzbrojony/rozbrojony
  Swdlautorun.txt na niewlasciwej karcie, znaczniki skopiowane za wczesnie.

  Komunikaty bez polskich znakow: PowerShell 5.1 czyta .ps1 bez BOM w stronie
  kodowej Windows.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\sprawdz_karte.ps1 -Karta E: -Typ ratunkowa
  powershell -ExecutionPolicy Bypass -File tools\sprawdz_karte.ps1 -Karta F: -Typ instalacyjna
  powershell -ExecutionPolicy Bypass -File tools\sprawdz_karte.ps1 -Karta F: -Typ etap4
  powershell -ExecutionPolicy Bypass -File tools\sprawdz_karte.ps1 -Karta I: -Typ etap7
#>
param(
    [Parameter(Mandatory = $true)][string]$Karta,
    [Parameter(Mandatory = $true)][ValidateSet('instalacyjna', 'ratunkowa', 'etap4', 'etap5', 'etap6', 'etap7')][string]$Typ
)

$ErrorActionPreference = 'Stop'
$script:bledy = 0
$script:uwagi = 0

function Pisz-OK($t)    { Write-Host "  OK     $t" -ForegroundColor Green }
function Pisz-Blad($t)  { $script:bledy++; Write-Host "  BLAD   $t" -ForegroundColor Red }
function Pisz-Uwaga($t) { $script:uwagi++; Write-Host "  UWAGA  $t" -ForegroundColor Yellow }
function Pisz-Info($t)  { Write-Host "  info   $t" }

$proj = Split-Path -Parent $PSScriptRoot
$pkgName = 'sd_ratunek'
if ($Typ -eq 'instalacyjna') { $pkgName = 'sd_etap3' }
# sd_etap5 = aktualna paczka (Etapy 4-6 w jednej); sd_etap4 to stara wersja.
if ($Typ -eq 'etap4' -or $Typ -eq 'etap5' -or $Typ -eq 'etap6') { $pkgName = 'sd_etap5' }
# Etap 7 = warstwa DOKLADANA na karte z Etapami 4-6, wiec ma wlasna, mala paczke.
if ($Typ -eq 'etap7') { $pkgName = 'sd_etap7' }
$pkg = Join-Path $proj (Join-Path 'work' $pkgName)

# "E:" -> "E:\"
if ($Karta -match '^[A-Za-z]:$') { $Karta = $Karta + '\' }
if (-not (Test-Path -LiteralPath $Karta)) {
    Write-Host "Nie ma takiej sciezki: $Karta (wlozona karta? dobra litera dysku?)" -ForegroundColor Red
    exit 2
}
$root = (Resolve-Path -LiteralPath $Karta).ProviderPath.TrimEnd('\')
function Na-Karcie($rel) { return (Join-Path $root $rel) }
# Etap 6b: opcjonalny adres BT MMI (BT_MAC). Adresu nie wypisujemy - to dane auta.
function Sprawdz-BtMac($dir) {
    $p = Join-Path $dir 'BT_MAC'
    if (-not (Test-Path -LiteralPath $p)) { Pisz-Info "brak BT_MAC - Etap 4 bez sekcji [bluetooth] (jak dotad)"; return }
    $bytes = [System.IO.File]::ReadAllBytes($p)
    $txt = [System.Text.Encoding]::ASCII.GetString($bytes)
    $first = ($txt -split "`n")[0].Trim()
    if ($first -cmatch '^[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}$' -and $first -ne '00:00:00:00:00:00' -and $first -ne 'FF:FF:FF:FF:FF:FF') {
        Pisz-OK "BT_MAC poprawny (format XX:XX:XX:XX:XX:XX) - INSTALL_ETAP4 doda sekcje [bluetooth]"
    } else { Pisz-Blad "BT_MAC ma zly format - INSTALL_ETAP4 odmowi (oczekiwano jednej linii XX:XX:XX:XX:XX:XX)" }
}

Write-Host ""
Write-Host "Sprawdzam karte $Typ : $Karta   (wzorzec: work\$pkgName)"
Write-Host "---------------------------------------------------------------"

# --- 1. System plikow ---
if ($Karta -match '^[A-Za-z]:\\$') {
    try {
        $fs = (Get-Volume -DriveLetter $Karta.Substring(0, 1)).FileSystem
        if ($fs -eq 'FAT32') { Pisz-OK "system plikow FAT32" }
        else { Pisz-Blad "system plikow '$fs' - jednostka wymaga FAT32 (sformatuj karte jako FAT32)" }
    } catch {
        Pisz-Uwaga "nie udalo sie odczytac systemu plikow ($($_.Exception.Message))"
    }
} else {
    Pisz-Info "to nie jest dysk, tylko folder - pomijam sprawdzenie systemu plikow"
}

# --- 2. Folder zamiast zawartosci ---
foreach ($f in @('sd_etap3', 'sd_etap4', 'sd_etap5', 'sd_etap7', 'sd_ratunek')) {
    if (Test-Path -LiteralPath (Na-Karcie $f)) {
        Pisz-Blad "na karcie jest folder '$f' - skopiowales FOLDER zamiast jego ZAWARTOSCI. Wejdz do work\$f, Ctrl+A, i wklej do katalogu glownego karty"
    }
}

# --- 3. Sumy kontrolne wzgledem paczki na komputerze ---
$sums = Join-Path $pkg 'SHA256SUMS.txt'
if (-not (Test-Path -LiteralPath $sums)) {
    Pisz-Blad "brak $sums na komputerze - nie ma z czym porownac"
} else {
    $n = 0; $zgodne = 0; $brak = @(); $rozne = @()
    foreach ($line in [System.IO.File]::ReadAllLines($sums)) {
        if ($line -notmatch '^([0-9a-f]{64}) \*\./(.+)$') { continue }
        $want = $Matches[1]; $rel = $Matches[2]
        # Uzbrojenie karty ratunkowej sprawdzane osobno nizej (plik zmienia nazwe przy uzyciu).
        if ($rel -eq 'Swdlautorun.txt') { continue }
        $n++
        $p = Na-Karcie ($rel -replace '/', '\')
        if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { $brak += $rel; continue }
        $got = (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLower()
        if ($got -eq $want) { $zgodne++ } else { $rozne += $rel }
    }
    if ($brak.Count -eq 0 -and $rozne.Count -eq 0) {
        Pisz-OK "wszystkie $n plikow paczki sa na karcie i maja zgodne sumy SHA-256"
    } else {
        Pisz-Blad "z $n plikow paczki: zgodnych $zgodne, brakujacych $($brak.Count), roznych $($rozne.Count)"
        foreach ($r in ($brak | Select-Object -First 15)) { Write-Host "           brak:   $r" -ForegroundColor Red }
        foreach ($r in ($rozne | Select-Object -First 15)) { Write-Host "           rozny:  $r" -ForegroundColor Red }
        if ($brak.Count -gt 0) { Pisz-Info "brakujace pliki M.I.B. czesto usuwa antywirus - sprawdz kwarantanne, potem skopiuj jeszcze raz" }
        if ($rozne.Count -gt 0) { Pisz-Info "rozne pliki = na karcie jest inna wersja paczki niz na komputerze - skopiuj karte jeszcze raz" }
    }
}

# --- 4. Sprawdzenia zalezne od typu karty ---
$armed    = Test-Path -LiteralPath (Na-Karcie 'Swdlautorun.txt')
$disarmed = Test-Path -LiteralPath (Na-Karcie '_Swdlautorun.txt')
$pkgDir   = Na-Karcie 'mod\mib2high-carplay-rgi-v8s-simple'

if ($Typ -eq 'etap7') {
    # Etap 7 = warstwa graficzna na VC (docs\18). Dokladana na karte, ktora juz
    # ma Etapy 4-6; dlatego sprawdzamy OBIE paczki, nie tylko swoja.
    if ((Test-Path -LiteralPath (Na-Karcie 'esd\scripts\individual.sh')) -and
        (Test-Path -LiteralPath (Na-Karcie 'esd\Launcher-sda0.esd'))) {
        Pisz-OK "jest M.I.B. - 'run individual script' bedzie dzialac"
    } else { Pisz-Blad "na karcie NIE MA M.I.B." }
    if ($armed) { Pisz-Blad "jest Swdlautorun.txt (bez podkreslenia) - zmien nazwe na _Swdlautorun.txt" }
    if ((Test-Path -LiteralPath (Join-Path $pkgDir 'installed.ok')) -and
        (Test-Path -LiteralPath (Join-Path $pkgDir 'backup\pre-v8s\complete'))) {
        Pisz-OK "v8s zainstalowany, kopia oryginalow backup\pre-v8s kompletna (NIE KASOWAC)"
    } else { Pisz-Blad "brak installed.ok albo backup\pre-v8s\complete" }
    if (-not (Test-Path -LiteralPath (Join-Path $pkgDir 'etap4.installed.ok'))) {
        Pisz-Blad "brak etap4.installed.ok - Etap 7 wymaga Etapu 4 (dane manewru z mm-ipod) i odmowi instalacji"
    }

    # Kolizja nazw w payloadzie potrafi cicho popsuc sprzatanie po Etapie 6:
    # Etap 4 i Etap 7 instaluja ten sam plik docelowy, ale MUSZA miec w payloadzie
    # rozne nazwy zrodlowe, inaczej INSTALL_ETAP4 wgra biblioteke Etapu 7.
    $libE4 = Join-Path $pkgDir 'payload\libmib2high-carplay-rgi-ipod.so'
    $libE7 = Join-Path $pkgDir 'payload\libmib2high-carplay-rgi-ipod.etap7.so'
    if (Test-Path -LiteralPath $libE7) {
        if (Test-Path -LiteralPath $libE4) {
            $h4 = (Get-FileHash -LiteralPath $libE4 -Algorithm SHA256).Hash.ToLower()
            $h7 = (Get-FileHash -LiteralPath $libE7 -Algorithm SHA256).Hash.ToLower()
            if ($h4 -eq $h7) {
                Pisz-Blad "payload Etapu 4 i Etapu 7 to ten sam plik - INSTALL_ETAP4 wgralby wersje Etapu 7, a ROLLBACK_ETAP7 nie mialby czego cofac"
            } else { Pisz-OK "biblioteki Etapu 4 i Etapu 7 w payloadzie sa rozne - transakcje rozdzielone" }
        } else { Pisz-Blad "brak payload\libmib2high-carplay-rgi-ipod.so (biblioteka Etapu 4)" }
    } else { Pisz-Blad "brak payload\libmib2high-carplay-rgi-ipod.etap7.so - skopiuj work\sd_etap7 jeszcze raz" }

    foreach ($f in @('vc_overlay', 'vc_overlay_probe', 'mib2high-overlay.conf', 'k3346-overlay-probe.sh')) {
        if (-not (Test-Path -LiteralPath (Join-Path $pkgDir "payload\$f"))) { Pisz-Blad "brak payload\$f" }
    }

    # Sprzatanie po Etapie 6 musi pojsc PRZED Etapem 7, bo INSTALL_ETAP4
    # nadpisuje ten sam plik docelowy.
    $czysc = @('ROLLBACK_ETAP4', 'INSTALL_ETAP4') | Where-Object { Test-Path -LiteralPath (Join-Path $pkgDir $_) }
    if ($czysc.Count -eq 2) {
        Pisz-OK "sprzatanie po Etapie 6 uzbrojone (ROLLBACK_ETAP4 + INSTALL_ETAP4) - to sa uruchomienia 1 i 2, PRZED Etapem 7"
    } elseif ($czysc.Count -eq 1) {
        Pisz-Uwaga "jest tylko $($czysc -join ', ') - sprzatanie po Etapie 6 potrzebuje OBU znacznikow"
    }

    $i7 = Test-Path -LiteralPath (Join-Path $pkgDir 'INSTALL_ETAP7')
    $p7 = Test-Path -LiteralPath (Join-Path $pkgDir 'PROBE_ETAP7')
    $r7 = Test-Path -LiteralPath (Join-Path $pkgDir 'ROLLBACK_ETAP7')
    if ($r7 -and $i7 -and $p7) {
        # Odswiezenie: pliki Etapu 7 sa juz w aucie, ale zmienily sie na karcie.
        # Dyspozytor bierze ROLLBACK_ETAP7 przed INSTALL_ETAP7 przed PROBE_ETAP7,
        # wiec komplet mozna wlozyc raz i w aucie tylko uruchamiac skrypt.
        Pisz-OK "ODSWIEZENIE Etapu 7 uzbrojone - 4 uruchomienia, bez wracania po karte:"
        Pisz-Info "   1. ROLLBACK_ETAP7 -> ETAP7_ROLLBACK_RESULT=PASS"
        Pisz-Info "   2. INSTALL_ETAP7  -> ETAP7_INSTALL_RESULT=PASS, potem RESTART MMI"
        Pisz-Info "   3. PROBE_ETAP7    -> sonda; iPhone ODLACZONY (renderer bierze ten sam displayable)"
        Pisz-Info "   -- potem podlacz iPhone i ustaw trase: nakladka wstaje sama"
        Pisz-Info "   4. bez znacznika  -> MODE=COLLECT"
    } elseif ($r7 -and $i7) {
        # Od wizyty 2 (2026-10-27) sonda nie jest juz czescia procedury: odpowiedziala
        # na swoje pytania, a jedynego otwartego - czy przezroczystosc przepuszcza mape
        # - zrzut nie pokaze, bo mapy nie ma w kompozycie. Stad odswiezenie BEZ sondy.
        Pisz-OK "ODSWIEZENIE Etapu 7 uzbrojone (bez sondy) - 3 uruchomienia, bez wracania po karte:"
        Pisz-Info "   1. ROLLBACK_ETAP7 -> ETAP7_ROLLBACK_RESULT=PASS"
        Pisz-Info "   2. INSTALL_ETAP7  -> ETAP7_INSTALL_RESULT=PASS, potem RESTART MMI"
        Pisz-Info "   -- potem podlacz iPhone i ustaw trase: nakladka wstaje sama"
        Pisz-Info "   -- patrz na VC OD RAZU: ma byc mapa. Czarne kombi = odlacz telefon"
        Pisz-Info "   3. bez znacznika  -> MODE=COLLECT"
    } elseif ($r7) {
        Pisz-OK "ROLLBACK_ETAP7 na miejscu - to uruchomienie cofnie Etap 7 (mapa, RGI, telefon i okladka zostaja)"
    } elseif ($czysc.Count -eq 2 -and $i7 -and $p7) {
        # Dyspozytor ma ustalona kolejnosc i zuzywa po jednym znaczniku na
        # uruchomienie, wiec komplet mozna wlozyc w domu - przy aucie nie jest
        # potrzebny czytnik kart. Sprawdzone w symulacji (T16l).
        Pisz-OK "PELNA SEKWENCJA uzbrojona - 5 uruchomien, bez wracania po karte:"
        Pisz-Info "   1. ROLLBACK_ETAP4   -> ETAP4_ROLLBACK_RESULT=PASS"
        Pisz-Info "   2. INSTALL_ETAP4    -> ETAP4_INSTALL_RESULT=PASS, ETAP4_BT=OFF"
        Pisz-Info "   3. INSTALL_ETAP7    -> ETAP7_INSTALL_RESULT=PASS, potem RESTART MMI"
        Pisz-Info "   4. PROBE_ETAP7      -> sonda ok. 1,5 min; iPhone ODLACZONY"
        Pisz-Info "   5. bez znacznika    -> MODE=COLLECT"
    } elseif ($i7 -and $p7) {
        Pisz-OK "INSTALL_ETAP7 + PROBE_ETAP7 - najpierw instalacja, sonda przy nastepnym uruchomieniu"
    } elseif ($i7) {
        if ($czysc.Count -eq 2) {
            Pisz-OK "sprzatanie Etapu 6 + INSTALL_ETAP7 - trzy uruchomienia pod rzad, kolejnosc poprawna (Etap 4 przed 7)"
        } else { Pisz-OK "INSTALL_ETAP7 na miejscu - to uruchomienie zainstaluje warstwe, potem restart MMI" }
    } elseif ($p7) {
        Pisz-OK "PROBE_ETAP7 na miejscu - sonda ok. 1,5 min, dwa razy poprosi: nacisnij VIEW, potem ocen schodek alfy. iPhone ma byc ODLACZONY: od Etapu 7b renderer i sonda biora ten sam displayable 98, a sonda sama odmowi, jesli renderer zyje"
    } else {
        if ($czysc.Count -eq 0) {
            Pisz-Uwaga "brak jakiegokolwiek znacznika Etapu 7 - to uruchomienie zrobi zwykly COLLECT. Wloz INSTALL_ETAP7 albo PROBE_ETAP7 z ZNACZNIKI\"
        }
    }

    foreach ($m in @('ROLLBACK', 'COLLECT_LOGS')) {
        if (Test-Path -LiteralPath (Join-Path $pkgDir $m)) { Pisz-Blad "jest znacznik $m - ma pierwszenstwo nad Etapem 7, usun go" }
    }
    Sprawdz-BtMac $pkgDir
    if (Test-Path -LiteralPath (Na-Karcie 'ratunek\ratunek.sh')) { Pisz-Blad "pliki karty ratunkowej na karcie instalacyjnej" }
} elseif ($Typ -eq 'etap6') {
    # Etap 6 = aktualizacja Etapu 4 (biblioteka mm-ipod + iap2.cfg) i 5 (JAR) naraz (docs\17).
    if ((Test-Path -LiteralPath (Na-Karcie 'esd\scripts\individual.sh')) -and
        (Test-Path -LiteralPath (Na-Karcie 'esd\Launcher-sda0.esd'))) {
        Pisz-OK "jest M.I.B. - 'run individual script' bedzie dzialac"
    } else { Pisz-Blad "na karcie NIE MA M.I.B." }
    if ($armed) { Pisz-Blad "jest Swdlautorun.txt (bez podkreslenia) - zmien nazwe na _Swdlautorun.txt" }
    if ((Test-Path -LiteralPath (Join-Path $pkgDir 'installed.ok')) -and
        (Test-Path -LiteralPath (Join-Path $pkgDir 'backup\pre-v8s\complete'))) {
        Pisz-OK "v8s zainstalowany, kopia oryginalow backup\pre-v8s kompletna (NIE KASOWAC)"
    } else { Pisz-Blad "brak installed.ok albo backup\pre-v8s\complete" }
    foreach ($e in @('etap4.installed.ok', 'etap5.installed.ok')) {
        if (-not (Test-Path -LiteralPath (Join-Path $pkgDir $e))) { Pisz-Blad "brak $e - Etap 6 aktualizuje zainstalowane Etapy 4 i 5" }
    }
    $brakZn = @()
    foreach ($m in @('ROLLBACK_ETAP5', 'ROLLBACK_ETAP4', 'INSTALL_ETAP4', 'INSTALL_ETAP5')) {
        if (-not (Test-Path -LiteralPath (Join-Path $pkgDir $m))) { $brakZn += $m }
    }
    if ($brakZn.Count -eq 0) {
        Pisz-OK "AKTUALIZACJA Etapow 4 i 5: ROLLBACK_ETAP5 + ROLLBACK_ETAP4 + INSTALL_ETAP4 + INSTALL_ETAP5 - run individual script CZTERY razy, potem restart MMI"
    } else { Pisz-Blad "brak znacznikow: $($brakZn -join ', ')" }
    foreach ($m in @('ROLLBACK', 'COLLECT_LOGS')) {
        if (Test-Path -LiteralPath (Join-Path $pkgDir $m)) { Pisz-Blad "jest znacznik $m - ma pierwszenstwo, usun go" }
    }
    Sprawdz-BtMac $pkgDir
    if (Test-Path -LiteralPath (Na-Karcie 'ratunek\ratunek.sh')) { Pisz-Blad "pliki karty ratunkowej na karcie instalacyjnej" }
} elseif ($Typ -eq 'etap4' -or $Typ -eq 'etap5') {
    # Karta instalacyjna z ZAINSTALOWANYM v8s, przygotowana do Etapu 4 (docs\15) albo 5 (docs\16).
    $nr = $Typ.Substring(4)
    if ((Test-Path -LiteralPath (Na-Karcie 'esd\scripts\individual.sh')) -and
        (Test-Path -LiteralPath (Na-Karcie 'esd\Launcher-sda0.esd'))) {
        Pisz-OK "jest M.I.B. - 'run individual script' bedzie dzialac"
    } else { Pisz-Blad "na karcie NIE MA M.I.B." }
    if ($armed) { Pisz-Blad "jest Swdlautorun.txt (bez podkreslenia) - zmien nazwe na _Swdlautorun.txt" }
    if ((Test-Path -LiteralPath (Join-Path $pkgDir 'installed.ok')) -and
        (Test-Path -LiteralPath (Join-Path $pkgDir 'backup\pre-v8s\complete'))) {
        Pisz-OK "v8s zainstalowany, kopia oryginalow backup\pre-v8s kompletna (NIE KASOWAC)"
    } else {
        Pisz-Blad "brak installed.ok albo backup\pre-v8s\complete - to nie jest karta, z ktorej instalowano v8s (Etap $nr wymaga v8s i jego kopii)"
    }
    $aktualizacja = (Test-Path -LiteralPath (Join-Path $pkgDir "etap$nr.installed.ok")) -and
        (Test-Path -LiteralPath (Join-Path $pkgDir "ROLLBACK_ETAP$nr")) -and
        (Test-Path -LiteralPath (Join-Path $pkgDir "INSTALL_ETAP$nr"))
    if ($aktualizacja) {
        Pisz-OK "AKTUALIZACJA Etapu ${nr}: ROLLBACK_ETAP$nr + INSTALL_ETAP$nr - run individual script DWA razy (1. cofa stara wersje, 2. instaluje nowa), potem restart MMI"
    } elseif (Test-Path -LiteralPath (Join-Path $pkgDir "etap$nr.installed.ok")) {
        Pisz-Info "Etap $nr juz zainstalowany (etap$nr.installed.ok) - kolejne uruchomienie zbierze logi"
    } elseif (Test-Path -LiteralPath (Join-Path $pkgDir "INSTALL_ETAP$nr")) {
        Pisz-OK "znacznik INSTALL_ETAP$nr na miejscu - run individual script zainstaluje Etap $nr"
    } else {
        Pisz-Uwaga "brak znacznika INSTALL_ETAP$nr w mod\mib2high-carplay-rgi-v8s-simple\ - bez niego skrypt tylko zbierze logi (skopiuj ZNACZNIKI\INSTALL_ETAP$nr)"
    }
    $blokujace = @('ROLLBACK', 'ROLLBACK_ETAP4', 'ROLLBACK_ETAP5', 'COLLECT_LOGS')
    if ($nr -eq '5') { $blokujace += 'INSTALL_ETAP4' }
    foreach ($m in $blokujace) {
        if ($aktualizacja -and $m -eq "ROLLBACK_ETAP$nr") { continue }
        if (Test-Path -LiteralPath (Join-Path $pkgDir $m)) {
            Pisz-Blad "w mod\mib2high-carplay-rgi-v8s-simple\ jest znacznik $m - ma pierwszenstwo przed instalacja Etapu $nr, usun go"
        }
    }
    if ($nr -eq '4') { Sprawdz-BtMac $pkgDir }
    if (Test-Path -LiteralPath (Na-Karcie 'ratunek\ratunek.sh')) { Pisz-Blad "pliki karty ratunkowej na karcie instalacyjnej" }
} elseif ($Typ -eq 'instalacyjna') {
    if ((Test-Path -LiteralPath (Na-Karcie 'esd\scripts\individual.sh')) -and
        (Test-Path -LiteralPath (Na-Karcie 'esd\Launcher-sda0.esd'))) {
        Pisz-OK "jest M.I.B. (esd\scripts\individual.sh) - 'run individual script' bedzie dzialac"
    } else {
        Pisz-Blad "na karcie NIE MA M.I.B. - paczke trzeba skopiowac na TWOJA karte z M.I.B., nie na czysta"
    }
    if ($armed) {
        Pisz-Blad "jest Swdlautorun.txt (bez podkreslenia) - zmien nazwe na _Swdlautorun.txt. Inaczej tryb aktualizacji M.I.B. wczytalby instalator"
    } elseif ($disarmed) {
        Pisz-OK "autorun rozbrojony (_Swdlautorun.txt)"
    } else {
        Pisz-Info "brak pliku autorun - tez dobrze (autorun nieaktywny)"
    }
    foreach ($m in @('ROLLBACK', 'COLLECT_LOGS')) {
        if (Test-Path -LiteralPath (Join-Path $pkgDir $m)) {
            Pisz-Blad "w mod\mib2high-carplay-rgi-v8s-simple\ jest znacznik $m - skopiowany za wczesnie, usun go (kopia zostaje w ZNACZNIKI\)"
        }
    }
    if ((Test-Path -LiteralPath (Join-Path $pkgDir 'installed.ok')) -or
        (Test-Path -LiteralPath (Join-Path $pkgDir 'backup\pre-v8s'))) {
        Pisz-Blad "na karcie jest slad instalacji (installed.ok / backup\pre-v8s). Jesli jeszcze nie instalowales - instalator odmowi ('existing backup retained'). Jesli instalowales - NIE KASUJ, to kopia oryginalow auta"
    }
    if (Test-Path -LiteralPath (Na-Karcie 'ratunek\ratunek.sh')) {
        Pisz-Blad "na karcie instalacyjnej sa pliki karty ratunkowej (ratunek\) - pomieszane karty"
    }
} else {
    if ($armed) {
        Pisz-OK "karta UZBROJONA (Swdlautorun.txt)"
        if ($disarmed) { Pisz-Uwaga "jest tez _Swdlautorun.txt - niepotrzebny, mozna usunac" }
    } elseif ($disarmed) {
        Pisz-Blad "karta ROZBROJONA (_Swdlautorun.txt) - zmien nazwe na Swdlautorun.txt (usun podkreslenie), inaczej w razie bootloopu nic nie zrobi"
    } else {
        Pisz-Blad "brak Swdlautorun.txt - karta nie uruchomi trybu aktualizacji"
    }
    if (Test-Path -LiteralPath (Na-Karcie 'mod\command.sh')) {
        Pisz-Blad "na karcie ratunkowej jest mod\command.sh - M.I.B. wczytalby go w trybie aktualizacji. Usun go"
    }
    if (Test-Path -LiteralPath $pkgDir) {
        Pisz-Blad "na karcie ratunkowej jest paczka instalacyjna (mod\mib2high-carplay-rgi-v8s-simple\) - pomieszane karty"
    }
    $log = Na-Karcie 'RATUNEK_LOG.txt'
    # Slady uzycia niezalezne od naszego logu: M.I.B. rozbraja karte i robi backup.
    $mibBackup = @(Get-ChildItem -LiteralPath (Na-Karcie 'backup') -Directory -Filter 'MU*' -ErrorAction SilentlyContinue)
    $uzyta = $disarmed -or ($mibBackup.Count -gt 0)
    if (Test-Path -LiteralPath $log) {
        $wyniki = @(Select-String -LiteralPath $log -Pattern '^WYNIK=' | ForEach-Object { $_.Line })
        $koniec = @(Select-String -LiteralPath $log -Pattern '^STUFF_KONIEC' | ForEach-Object { $_.Line })
        Pisz-Info "karta byla juz uzyta - RATUNEK_LOG.txt: $($wyniki.Count) wynik(ow), $($koniec.Count) slad(ow) STUFF_KONIEC"
        foreach ($w in $wyniki) { Pisz-Info "   $w" }
        foreach ($k in $koniec) { Pisz-Info "   $k" }
        if ($wyniki.Count -eq 0) {
            Pisz-Blad "w logu jest slad M.I.B., ale ZADNEGO wyniku ratunku - ratunek.sh sie nie wykonal. Pokaz mi RATUNEK_LOG.txt"
        } elseif ($wyniki[-1] -notmatch '^WYNIK=OK') {
            Pisz-Uwaga "ostatni przebieg nie dal WYNIK=OK - pokaz mi RATUNEK_LOG.txt"
        }
    } elseif ($uzyta) {
        Pisz-Blad "karta BYLA uzyta (M.I.B. ja rozbroil / zrobil backup), ale ratunek NIE zostawil logu - ratunek nie zadzialal"
    } else {
        Pisz-Info "brak RATUNEK_LOG.txt - karta jeszcze nie byla uzyta"
    }
}

Write-Host "---------------------------------------------------------------"
if ($script:bledy -eq 0) {
    Write-Host "WYNIK: karta $Typ GOTOWA ($script:uwagi uwag)" -ForegroundColor Green
    exit 0
} else {
    Write-Host "WYNIK: $script:bledy BLEDOW, $script:uwagi uwag - karta $Typ NIE jest gotowa" -ForegroundColor Red
    exit 1
}
