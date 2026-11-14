#!/usr/bin/env python3
"""Tests of tools/cpu_report.py:  python tools/test_cpu_report.py

The "pidin times" and "pidin info" fixtures are lines of the first measurement in
the car (2.0.0-rc4, 2026-11-11, CarPlay with a route): a few system processes out
of 141, so the window they give is shorter than the 12 s of the whole file.
"""
import io
import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cpu_report as c  # noqa: E402

ok = fail = 0


def eq(name, want, got):
    global ok, fail
    if want == got:
        ok += 1
    else:
        fail += 1
        print('  FAIL %s: chcialem %r, jest %r' % (name, want, got))


def near(name, want, got, tol=0.0006):
    eq(name, True, got is not None and abs(want - got) <= tol)


HEAD = '     pid name                   sid  start time   utime  stime cutime cstime\n'
T1 = HEAD + '''       1 /procnto-smp-instr       1 Jan 01 00:00  9m10s 24.707  0.011  0.002
  163861 sbin/devg-nvrm           1 Jan 01 00:00 35.931 13.710  0.000  0.000
  450584 sbin/io-pkt-v6-hc        1 Jan 01 00:00 15.611  2.690  0.000  0.000
 1171504 so/bin/traceserver       1 Jan 01 00:00 30.217  1.046  0.000  0.000
 1310776 ifs/jre/bin/j9           1 Jan 01 00:00  2m24s  5.813  0.000  0.000
 1384508 bin/sh                   1 Jan 01 00:00  0.005  0.000  0.002  0.002
 3199103 n/apps/dio_manager       1 Jan 01 00:00  8.453  1.701  0.000  0.000
 3207296 e/usr/sbin/mm-ipod       1 Jan 01 00:00  1.609  0.299  0.000  0.000
 3260481 bin/sh                   1 Jan 01 00:01  0.557  0.121 19.692  1.545
 3285121 -dsitracer-capture       1 Jan 01 00:01  0.027  0.004  0.000  0.000
 3342468 app/root/vc_render       1 Jan 01 00:01  1.964  0.428  0.000  0.000
 3412104 pp/root/vc_overlay       1 Jan 01 00:01 17.068  5.086  0.000  0.000
'''
T2 = HEAD + '''       1 /procnto-smp-instr       1 Jan 01 00:00  9m32s 25.540  0.011  0.002
  163861 sbin/devg-nvrm           1 Jan 01 00:00 38.179 14.660  0.000  0.000
  450584 sbin/io-pkt-v6-hc        1 Jan 01 00:00 16.463  2.861  0.000  0.000
 1171504 so/bin/traceserver       1 Jan 01 00:00 31.373  1.080  0.000  0.000
 1310776 ifs/jre/bin/j9           1 Jan 01 00:00  2m37s  6.014  0.000  0.000
 1384508 bin/sh                   1 Jan 01 00:00  0.005  0.000  0.002  0.002
 3199103 n/apps/dio_manager       1 Jan 01 00:00  8.812  1.814  0.000  0.000
 3207296 e/usr/sbin/mm-ipod       1 Jan 01 00:00  1.698  0.310  0.000  0.000
 3260481 bin/sh                   1 Jan 01 00:01  0.595  0.124 20.770  1.630
 3285121 -dsitracer-capture       1 Jan 01 00:01  0.027  0.004  0.000  0.000
 3342468 app/root/vc_render       1 Jan 01 00:01  2.060  0.448  0.000  0.000
 3412104 pp/root/vc_overlay       1 Jan 01 00:01 18.231  5.500  0.000  0.000
'''
AR = '''     pid Arguments
       1 procnto-smp-instr -v -mL -F 9000
  163861 devg-nvrm
  450584 io-pkt-v6-hc -ptcpip stacksize=8192
 1171504 /eso/bin/traceserver
 1310776 /ifs/jre/bin/j9 -Dipl.config.dir=/etc/eso/production -Dipl.config.myProcName=hmi -Xquickstart
 1384508 /bin/sh /etc/boot/startup.sh
 3199103 /mnt/app/eso/bin/apps/dio_manager
 3207296 /mnt/app/armle/usr/sbin/mm-ipod -d iap2,config=/etc/mm/iap2.cfg,probe
 3260481 /bin/sh /mnt/app/root/mib2high-lvds-control.sh
 3285121 /mnt/app/root/v8l-dsitracer-capture 1798
 3342468 /mnt/app/root/vc_render --out 1 --layer 58 --w 1440 --h 542 --fps 30 --ts wall
 3412104 /mnt/app/root/vc_overlay
'''
INFO = '''CPU:ARM Release:6.5.0  FreeMem:785Mb/2047Mb BootTime:Jan 01 00:00:00 GMT 1970
Processes: 139, Threads: 2319
Processor1: 1093648537 Cortex A9 MPCore 900MHz FPU
Processor2: 1093648537 Cortex A9 MPCore 900MHz FPU
Processor3: 1093648537 Cortex A9 MPCore 900MHz FPU
Processor4: 1093648537 Cortex A9 MPCore 900MHz FPU
'''
LS = '''total 10089
nrw-------  4 root      root          60373 Jan 01 00:00 IBRConfigSHM
nrw-rw-rw-  1 root      root         338537 Jan 01 00:05 iap2.log
nrw-------  1 root      root         326674 Jan 01 00:05 k3346-rgi-ipod-native.log
drwxr-xr-x  2 root      root              0 Jan 01 00:00 dir
'''
TOP_HELP = '''top: illegal option -- b
usage: /mnt/app/armle/usr/bin/top [-i <number>] [-d] [-n <node>]
 -d         dumb terminal
'''

# "pidin ttimes" and the matching "pidin times" lines of the 2.0.0-rc6 measurement
# (2026-11-11, CarPlay with a route).  Thread 157 of j9 is the green menu waiting
# for the script: started in the minute the script was, a whole core ever since.
THEAD = '     pid tid name               STATE        start time  thread start sutime\n'
H1 = THEAD + '''       1   1 /procnto-smp-instr READY       Jan 01 00:00 Jan 01 00:00  4m50s
       1   2 /procnto-smp-instr RUNNING     Jan 01 00:00 Jan 01 00:00  4m57s
       1   3 /procnto-smp-instr READY       Jan 01 00:00 Jan 01 00:00  4m58s
       1   4 /procnto-smp-instr RUNNING     Jan 01 00:00 Jan 01 00:00  4m58s
  397334   2 sbin/io-pkt-v6-hc  RECEIVE     Jan 01 00:00 Jan 01 00:00 91.708
 1171502   4 so/bin/traceserver CONDVAR     Jan 01 00:00 Jan 01 00:00 66.904
 1306679   1 ifs/jre/bin/j9     CONDVAR     Jan 01 00:00 Jan 01 00:00  1.362
 1306679  31 ifs/jre/bin/j9     CONDVAR     Jan 01 00:00 Jan 01 00:00 25.842
 1306679 118 ifs/jre/bin/j9     REPLY       Jan 01 00:00 Jan 01 00:00 19.354
 1306679 155 ifs/jre/bin/j9     CONDVAR     Jan 01 00:00 Jan 01 00:10  0.000
 1306679 157 ifs/jre/bin/j9     RUNNING     Jan 01 00:00 Jan 01 00:10  5.990
'''
H2 = THEAD + '''       1   1 /procnto-smp-instr RUNNING     Jan 01 00:00 Jan 01 00:00  5m01s
       1   2 /procnto-smp-instr RUNNING     Jan 01 00:00 Jan 01 00:00  5m07s
       1   3 /procnto-smp-instr READY       Jan 01 00:00 Jan 01 00:00  5m08s
       1   4 /procnto-smp-instr READY       Jan 01 00:00 Jan 01 00:00  5m10s
  397334   2 sbin/io-pkt-v6-hc  RECEIVE     Jan 01 00:00 Jan 01 00:00 93.054
 1171502   4 so/bin/traceserver CONDVAR     Jan 01 00:00 Jan 01 00:00 68.522
 1306679   1 ifs/jre/bin/j9     CONDVAR     Jan 01 00:00 Jan 01 00:00  1.370
 1306679  31 ifs/jre/bin/j9     CONDVAR     Jan 01 00:00 Jan 01 00:00 26.460
 1306679 118 ifs/jre/bin/j9     REPLY       Jan 01 00:00 Jan 01 00:00 20.067
 1306679 155 ifs/jre/bin/j9     CONDVAR     Jan 01 00:00 Jan 01 00:10  0.000
 1306679 157 ifs/jre/bin/j9     RUNNING     Jan 01 00:00 Jan 01 00:10 27.210
couldn't fill_status()
08
 1757276     (Zombie)                       Jan 01 00:00
'''
P1 = HEAD + '''       1 /procnto-smp-instr       1 Jan 01 00:00 21m49s 53.930  0.016  0.001
  397334 sbin/io-pkt-v6-hc        1 Jan 01 00:00 88.327 16.518  0.000  0.000
 1171502 so/bin/traceserver       1 Jan 01 00:00 79.065  2.539  0.000  0.000
 1306679 ifs/jre/bin/j9           1 Jan 01 00:00  1m57s 13.965  1.648  0.043
'''
P2 = HEAD + '''       1 /procnto-smp-instr       1 Jan 01 00:00 22m34s 55.196  0.016  0.001
  397334 sbin/io-pkt-v6-hc        1 Jan 01 00:00 89.582 16.762  0.000  0.000
 1171502 so/bin/traceserver       1 Jan 01 00:00 80.828  2.606  0.000  0.000
 1306679 ifs/jre/bin/j9           1 Jan 01 00:00  2m20s 14.294  1.648  0.043
'''
AR5 = '''     pid Arguments
       1 procnto-smp-instr -v -mL -F 9000
  397334 io-pkt-v6-hc -S -ptcpip stacksize=8192
 1171502 /eso/bin/traceserver
 1306679 /ifs/jre/bin/j9 -Dipl.config.dir=/etc/eso/production -Dipl.config.myProcName=hmi -Xquickstart
'''
# by hand: procnto 45 + 1.266, io-pkt 1.255 + 0.244, traceserver 1.763 + 0.067, j9 23 + 0.329
P_TOTAL = 46.266 + 1.499 + 1.830 + 23.329
P_WINDOW = P_TOTAL / 4                                             # 18.231
P_BUSY = 100.0 * (P_TOTAL - 46.266) / P_WINDOW                     # percent of one core, all cores
D_PROBE = 27.210 - 5.990                                           # 21.220, more than the window

# deltas of the excerpt, by hand
D_OVERLAY = (18.231 - 17.068) + (5.500 - 5.086)                    # 1.577
D_CTL_OWN = (0.595 - 0.557) + (0.124 - 0.121)                      # 0.041
D_CTL_KIDS = (20.770 - 19.692) + (1.630 - 1.545)                   # 1.163
D_J9 = 13 + (6.014 - 5.813)                                        # 13.201
D_KERNEL_IDLE = 22 + (25.540 - 24.707)                             # 22.833
D_REST = ((38.179 - 35.931) + (14.660 - 13.710) + (16.463 - 15.611) + (2.861 - 2.690)
          + (31.373 - 30.217) + (1.080 - 1.046) + (8.812 - 8.453) + (1.814 - 1.701)
          + (1.698 - 1.609) + (0.310 - 0.299) + (2.060 - 1.964) + (0.448 - 0.428))
TOTAL = D_OVERLAY + D_CTL_OWN + D_CTL_KIDS + D_J9 + D_KERNEL_IDLE + D_REST
WINDOW = TOTAL / 4

# ---- times
eq('sekundy z milisekundami', (24.707, 0.001), c.parse_time('24.707'))
eq('minuty i sekundy', (550.0, 1.0), c.parse_time('9m10s'))
eq('godziny i minuty', (3720.0, 60.0), c.parse_time('1h02m'))
eq('same sekundy z litera', (59.0, 1.0), c.parse_time('59s'))
eq('dni i godziny', (93600.0, 3600.0), c.parse_time('1d02h'))
eq('godzina startu to nie czas', None, c.parse_time('00:00'))
eq('liczba calkowita to nie czas', None, c.parse_time('163861'))
eq('pusty', None, c.parse_time(''))
eq('smieci', None, c.parse_time('9x10s'))

t1, t2 = c.parse_times(T1), c.parse_times(T2)
eq('procesy w migawce', 12, len(t1))
eq('naglowek pominiety', False, 0 in t1)
eq('nazwa', 'pp/root/vc_overlay', t1[3412104]['name'])
near('wlasny czas = utime + stime', 22.154, t1[3412104]['own'])
near('czas dzieci = cutime + cstime', 21.237, t1[3260481]['kids'])
near('czas w minutach', 144 + 5.813, t1[1310776]['own'])
eq('rozdzielczosc sekundowa', 1.0, t1[1310776]['res'])
eq('rozdzielczosc milisekundowa', 0.001, t1[3412104]['res'])
eq('linia z pomoca to nie proces', {}, c.parse_times(TOP_HELP))
eq('stary format na watek nie jest brany za proces', {},
   c.parse_times(' 6049924   1 vc_render          Jan 01 00:13        10.000    2.000    0.000    0.000\n'))
eq('rdzenie', 4, c.cores_from_info(INFO))
eq('brak rdzeni', None, c.cores_from_info('nic'))
eq('wolna pamiec', ('785Mb', '2047Mb'), c.free_mem_from_info(INFO))

args = c.parse_args(AR)
eq('skrypt za sh', 'mib2high-lvds-control.sh', c.label(3260481, 'bin/sh', args))
eq('inny skrypt za sh to inny wiersz', 'startup.sh', c.label(1384508, 'bin/sh', args))
eq('j9 z nazwa procesu', 'j9 (hmi)', c.label(1310776, 'ifs/jre/bin/j9', args))
eq('binarka bez sciezki', 'vc_render', c.label(3342468, 'app/root/vc_render', args))
eq('bez pidin ar: nazwa z migawki', 'vc_overlay', c.label(99, 'pp/root/vc_overlay', {}))
eq('nasz proces', 'nasz', c.whose('vc_overlay'))
eq('nasz skrypt', 'nasz', c.whose('mib2high-lvds-control.sh'))
eq('fabryczny z naszym kodem', 'fabryczny z naszym kodem', c.whose('j9 (hmi)'))
eq('fabryczny', '', c.whose('traceserver'))

m = c.measure(t1, t2, 4, args)
near('okno = suma roznic (z dziecmi) / rdzenie', WINDOW, m['window'])
near('strzalka', D_OVERLAY, m['rows']['vc_overlay']['own'])
near('skrypt: wlasny czas', D_CTL_OWN, m['rows']['mib2high-lvds-control.sh']['own'])
near('skrypt: dzieci', D_CTL_KIDS, m['rows']['mib2high-lvds-control.sh']['kids'])
near('j9', D_J9, m['rows']['j9 (hmi)']['own'])
eq('j9: rozdzielczosc sekundowa przechodzi do wiersza', 1.0, m['rows']['j9 (hmi)']['res'])
eq('strzalka: milisekundy', 0.001, m['rows']['vc_overlay']['res'])
near('jadro i bezczynnosc osobno', D_KERNEL_IDLE, m['kernel_idle'])
eq('procnto nie jest wierszem', False, any('procnto' in n for n in m['rows']))
near('obciazenie bez procnto', 100.0 * (TOTAL - D_KERNEL_IDLE) / TOTAL, c.busy(m), 0.001)
eq('bez rdzeni: brak wyniku', None, c.measure(t1, t2, None, args))
gone = dict(t2)
del gone[3412104]
eq('proces, ktory zniknal, pominiety', False, 'vc_overlay' in c.measure(t1, gone, 4, args)['rows'])
new = dict(t1)
del new[3412104]
eq('proces, ktory przybyl, pominiety', False, 'vc_overlay' in c.measure(new, t2, 4, args)['rows'])
back = dict(t2)
back[3412104] = dict(t2[3412104], own=1.0)
eq('ujemna roznica pominieta', False, 'vc_overlay' in c.measure(t1, back, 4, args)['rows'])
two = c.measure(t1, t2, 4, {})
eq('bez pidin ar oba sh w jednym wierszu', True, 'sh' in two['rows'])
near('...z suma dzieci', D_CTL_KIDS, two['rows']['sh']['kids'])

# ---- threads
h1, h2 = c.parse_ttimes(H1), c.parse_ttimes(H2)
eq('watki w migawce', 11, len(h1))
eq('smieci, zombie i naglowek pominiete', 11, len(h2))
near('czas watku', 5.990, h1[(1306679, 157)]['time'])
eq('czas watku w minutach', (290.0, 1.0), (h1[(1, 1)]['time'], h1[(1, 1)]['res']))
eq('nazwa watku', 'ifs/jre/bin/j9', h1[(1306679, 157)]['name'])
eq('watek powstal pozniej niz proces', True, h1[(1306679, 157)]['late'])
eq('...i kiedy', 'Jan 01 00:10', h1[(1306679, 157)]['born'])
eq('watek z poczatku procesu', False, h1[(1306679, 118)]['late'])
eq('linia "pidin times" to nie watek', {}, c.parse_ttimes(T1))
eq('pusty plik', {}, c.parse_ttimes(None))
eq('bez godziny startu to nie watek', {},
   c.parse_ttimes(' 1306679 157 ifs/jre/bin/j9     RUNNING     Jan 01 0000 Jan 01 00:10 27.210\n'))
eq('linia bez stanu to nie watek', {},
   c.parse_ttimes(' 1306679 157 ifs/jre/bin/j9                 Jan 01 00:00 Jan 01 00:10 27.210\n'))
a5 = c.parse_args(AR5)
th = c.threads(h1, h2, a5)
eq('watki od najciezszego, bez procnto i bez stojacych',
   [('j9 (hmi)', 157), ('traceserver', 4), ('io-pkt-v6-hc', 2), ('j9 (hmi)', 118), ('j9 (hmi)', 31), ('j9 (hmi)', 1)],
   [(r['label'], r['tid']) for r in th])
near('czas watku w oknie', D_PROBE, th[0]['sec'])
near('czas watku przed oknem zostaje', 27.210, th[0]['total'])
eq('watek, ktory przybyl, pominiety', False,
   any(r['tid'] == 157 for r in c.threads({k: v for k, v in h1.items() if k[1] != 157}, h2, a5)))
eq('watek, ktory zniknal, pominiety', False,
   any(r['tid'] == 157 for r in c.threads(h1, {k: v for k, v in h2.items() if k[1] != 157}, a5)))
probe = c.probe_thread(th, P_WINDOW)
eq('watek pomiaru znaleziony', 157, probe and probe['tid'])


def changed(**kw):
    return [dict(th[0], **kw)] + th[1:]


eq('watek z poczatku procesu to nie pomiar', None, c.probe_thread(changed(late=False), P_WINDOW))
eq('watek innego procesu to nie pomiar', None, c.probe_thread(changed(label='traceserver'), P_WINDOW))
eq('watek ponizej 90% okna to nie pomiar', None, c.probe_thread(changed(sec=0.89 * P_WINDOW, total=20.0), P_WINDOW))
eq('...a od 90% tak', 157, c.probe_thread(changed(sec=0.9 * P_WINDOW, total=20.0), P_WINDOW)['tid'])
eq('watek, ktory pracowal dlugo przed pomiarem, to nie pomiar', None,
   c.probe_thread(changed(total=D_PROBE + 60.0), P_WINDOW))
eq('...a z niecala minuta tak', 157, c.probe_thread(changed(total=D_PROBE + 59.9), P_WINDOW)['tid'])
eq('bez watkow', None, c.probe_thread([], P_WINDOW))

# ---- shmem
files = c.shmem_files(LS)
eq('pliki od najwiekszego, bez katalogow', [(338537, 'iap2.log'), (326674, 'k3346-rgi-ipod-native.log'),
                                         (60373, 'IBRConfigSHM')], files)
eq('puste', [], c.shmem_files(None))

# ---- whole report
d = tempfile.mkdtemp()
try:
    def put(run, name, text):
        p = os.path.join(d, run, name)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, 'w', encoding='utf-8', newline='') as f:
            f.write(text)

    os.makedirs(os.path.join(d, 'stary'))
    code, lines = c.report(os.path.join(d, 'stary'))
    eq('bez katalogu cpu/: kod 2', 2, code)
    eq('...z wyjasnieniem', True, 'BRAK katalogu cpu/' in '\n'.join(lines))

    put('zly', 'cpu/status.txt', 'CPU_WINDOW=20\nPIDIN_TIMES_1=FAILED\n')
    put('zly', 'cpu/pidin-times-1.txt', 'sh: pidin: cannot execute\n')
    put('zly', 'cpu/pidin-times-2.txt', '')
    code, lines = c.report(os.path.join(d, 'zly'))
    text = '\n'.join(lines)
    eq('nieczytelne migawki: kod 2', 2, code)
    eq('...z poczatkiem surowego pliku', True, '| sh: pidin: cannot execute' in text and 'PIDIN_TIMES_1=FAILED' in text)

    # "bez": the overlay moved by 0.1 s and j9 as much as with CarPlay, so j9 is the
    # biggest row and the smallest difference
    quiet = T1.replace('17.068  5.086', '17.068  5.186').replace('2m24s  5.813', '2m37s  6.014')
    for run, a, b in (('z', T1, T2), ('bez', T1, quiet)):
        put(run, 'pidin-ar.txt', AR)
        put(run, 'cpu/status.txt', 'CPU_WINDOW=12\nTOP=EXIT_0\nPIDIN_TIMES_1=OK\nPIDIN_TIMES_2=OK\n')
        put(run, 'cpu/pidin-times-1.txt', T1 if run == 'z' else a)
        put(run, 'cpu/pidin-times-2.txt', (b if run == 'z' else b).replace('\n', '\r\n'))
        put(run, 'cpu/pidin-info.txt', INFO)
        put(run, 'cpu/ls-shmem.txt', LS)
        put(run, 'cpu/top.txt', TOP_HELP)
    code, lines = c.report(os.path.join(d, 'z'))
    text = '\n'.join(lines)
    eq('pelny raport: kod 0', 0, code)
    pct = lambda s: 100.0 * s / WINDOW   # noqa: E731
    for want in ('Okno: %.1f s, rdzeni: 4' % WINDOW,
                 'Obciazenie jednostki bez jadra i bezczynnosci: %.0f%% z 400%%' % (400.0 * (TOTAL - D_KERNEL_IDLE) / TOTAL),
                 'Wolna pamiec: 785Mb z 2047Mb',
                 '%7.1f%% %7.1f%%  vc_overlay   <- nasz' % (pct(D_OVERLAY), 0.0),
                 '%7.1f%% %7.1f%%  mib2high-lvds-control.sh   <- nasz' % (pct(D_CTL_OWN), pct(D_CTL_KIDS)),
                 '%7.1f%% %7.1f%%  j9 (hmi)   <- fabryczny z naszym kodem   (+-%.0f%%: czas w calych sekundach)'
                 % (pct(D_J9), 0.0, pct(1.0)),
                 'Nasze procesy razem: %.1f%% jednego rdzenia' % pct(D_OVERLAY + D_CTL_OWN + D_CTL_KIDS + 0.116),
                 'Pliki w /dev/shmem: 3, razem 0.7 MB',
                 '338537 B  iap2.log'):
        eq('raport zawiera: ' + want.strip()[:60], True, want in text)
    eq('top.txt ze starych przebiegow pominiety', False, 'illegal option' in text or 'top.txt' in text)
    eq('bez czasow na watek: bez listy watkow i bez odejmowania', False,
       'watki' in text or 'sam pomiar' in text)
    eq('procnto nie jest na liscie', False, 'procnto' in text)
    eq('kolejnosc: najwiekszy pierwszy', True, text.index('j9 (hmi)') < text.index('vc_overlay   <-'))

    code, lines = c.report(os.path.join(d, 'z'), os.path.join(d, 'bez'))
    text = '\n'.join(lines)
    eq('porownanie: kod 0', 0, code)
    wb = (0.1 + D_J9) / 4
    eq('porownanie: wiersz strzalki z roznica', True,
       ('%7.1f%% %7.1f%% %7.1f%% %+7.1f%%  vc_overlay   <- nasz' % (pct(D_OVERLAY), 0.0, 100.0 * 0.1 / wb, pct(D_OVERLAY) - 100.0 * 0.1 / wb)) in text)
    eq('porownanie: obciazenie drugiego przebiegu', True, '(bez CarPlay: 400%)' in text)
    eq('porownanie: pamiec drugiego przebiegu', True, '(bez CarPlay: 785Mb)' in text)
    eq('porownanie: kolejnosc wedlug roznicy, nie wielkosci', True,
       text.index('devg-nvrm') < text.index('vc_overlay   <-') < text.index('j9 (hmi)'))
    code, lines = c.report(os.path.join(d, 'z'), os.path.join(d, 'stary'))
    eq('porownanie z przebiegiem bez pomiaru: kod 0, sam pierwszy', True,
       code == 0 and 'Drugi przebieg bez pomiaru' in '\n'.join(lines))

    # with thread times: the measurement thread is named and taken off the load
    for run in ('w', 'wbez'):
        put(run, 'pidin-ar.txt', AR5)
        put(run, 'cpu/status.txt', 'CPU_WINDOW=20\nPIDIN_TIMES_1=OK\nPIDIN_TIMES_2=OK\n')
        put(run, 'cpu/pidin-times-1.txt', P1)
        put(run, 'cpu/pidin-times-2.txt', P2)
        put(run, 'cpu/pidin-ttimes-1.txt', H1)
        put(run, 'cpu/pidin-ttimes-2.txt', H2.replace('\n', '\r\n'))
        put(run, 'cpu/pidin-info.txt', INFO)
    code, lines = c.report(os.path.join(d, 'w'))
    text = '\n'.join(lines)
    p5 = lambda s: 100.0 * s / P_WINDOW   # noqa: E731
    eq('raport z watkami: kod 0', 0, code)
    for want in ('Obciazenie jednostki bez jadra i bezczynnosci: %.0f%% z 400%%' % P_BUSY,
                 'W tym sam pomiar (watek zielonego menu czeka na skrypt): 100%%, bez niego: %.0f%% z 400%%' % (P_BUSY - 100.0),
                 'j9 (hmi)   <- fabryczny z naszym kodem   (+-%.0f%%: czas w calych sekundach)   (w tym 100%% to sam pomiar)' % p5(1.0),
                 'Najciezsze watki (procent jednego rdzenia',
                 '%7.1f%%  j9 (hmi) watek 157, powstal Jan 01 00:10   <- sam pomiar: zielone menu czeka na skrypt' % p5(D_PROBE),
                 '%7.1f%%  traceserver watek 4' % p5(68.522 - 66.904),
                 '%7.1f%%  j9 (hmi) watek 118' % p5(20.067 - 19.354)):
        eq('raport z watkami zawiera: ' + want.strip()[:60], True, want in text)
    eq('watek z poczatku procesu bez daty powstania', False, 'watek 118, powstal' in text)
    eq('wiersz traceserver bez dopisku o pomiarze', False,
       any('traceserver' in line and 'sam pomiar' in line for line in lines))
    eq('lista watkow: najciezszy pierwszy', True, text.index('watek 157') < text.index('watek 4') < text.index('watek 118'))
    code, lines = c.report(os.path.join(d, 'w'), os.path.join(d, 'wbez'))
    eq('porownanie: obciazenie drugiego przebiegu bez pomiaru', True,
       ('bez niego: %.0f%% z 400%%   (bez CarPlay: %.0f%%)' % (P_BUSY - 100.0, P_BUSY - 100.0)) in '\n'.join(lines))
    code, lines = c.report(os.path.join(d, 'w'), os.path.join(d, 'z'))
    eq('porownanie z przebiegiem bez czasow na watek', True,
       '(bez CarPlay: brak czasow na watek)' in '\n'.join(lines))
    code, lines = c.report(os.path.join(d, 'z'), os.path.join(d, 'w'))
    eq('pierwszy przebieg bez watkow: bez linii o pomiarze', False, 'sam pomiar' in '\n'.join(lines))

    # the thread is in the list but is not the measurement: nothing taken off
    put('w', 'cpu/pidin-ttimes-2.txt', H2.replace('Jan 01 00:10 27.210', 'Jan 01 00:00 27.210'))
    code, lines = c.report(os.path.join(d, 'w'))
    text = '\n'.join(lines)
    eq('zwykly ciezki watek: na liscie, bez odejmowania', True,
       code == 0 and 'j9 (hmi) watek 157' in text and 'sam pomiar' not in text)
    put('w', 'cpu/pidin-ttimes-2.txt', 'pidin: unknown format\n')
    code, lines = c.report(os.path.join(d, 'w'))
    text = '\n'.join(lines)
    eq('nieczytelne czasy na watek: raport i tak jest', True, code == 0 and 'Okno: %.1f s' % P_WINDOW in text)
    eq('...z uwaga i bez listy watkow', True,
       'pidin ttimes: nieznany format' in text and 'Najciezsze watki' not in text)
finally:
    shutil.rmtree(d)

err = sys.stderr
sys.stderr = io.StringIO()
try:
    eq('bez argumentow: kod 1', 1, c.main(['cpu_report.py']))
    eq('--bez bez katalogu: kod 1', 1, c.main(['cpu_report.py', 'x', '--bez']))
finally:
    sys.stderr = err
print('WYNIK_CPU_REPORT: %d OK, %d FAIL' % (ok, fail))
sys.exit(1 if fail else 0)
