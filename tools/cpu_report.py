#!/usr/bin/env python3
"""CPU and memory report from a collected run (run-NN/cpu/, written by custom.sh).

    python tools/cpu_report.py <run-NN>                  one run
    python tools/cpu_report.py <run-NN> --bez <run-MM>   the same, next to a run
                                                         without CarPlay

The car only saves raw command output; the numbers are worked out here.

"pidin times" on this unit (MHI2 Harman K3346, QNX 6.5; first seen 2026-11-11)
is one line a PROCESS:

     pid name                   sid  start time   utime  stime cutime cstime
       1 /procnto-smp-instr       1 Jan 01 00:00  9m10s 24.707  0.011  0.002

Times under a minute have milliseconds, longer ones are "9m10s": whole seconds.
utime + stime is the process itself, cutime + cstime its children that have
ended, which is where a shell script's work shows (each command it runs).
procnto's own time is the kernel AND the idle threads, so the load of the unit
is everything else.  The window is the sum of all differences divided by the
cores; in the car that gave 12.0 s for a 12 s wait.

"pidin ttimes" (from 2.0.0-rc6 on) is one line a THREAD:

     pid tid name               STATE        start time  thread start sutime
 1306679 157 ifs/jre/bin/j9     RUNNING     Jan 01 00:00 Jan 01 00:10 27.210

Its two snapshots are taken outside the "pidin times" ones, so their window is
about a second longer.  They showed what "the HMI takes a whole core, CarPlay or
not" was: ONE j9 thread, started in the minute the script was started, running
flat out for the whole window.  That is the green menu waiting for our script,
so the measurement itself; the report names it and gives the load without it.

top is not used: on the unit it needs a terminal ("Unable to enter cbreak
mode"), and before that fix it only printed its help.
Exit code 2 when the two "pidin times" snapshots cannot be read.
"""
import os
import re
import sys

# Our own processes, and stock ones that carry our code.
OURS = ('vc_render', 'vc_overlay', 'mib2high-lvds-control', 'dsitracer-capture', 'k3346-')
HOSTS = ('mm-ipod', 'dio_manager', 'j9')
UNITS = (('d', 86400.0), ('h', 3600.0), ('m', 60.0), ('s', 1.0))


def read(path):
    try:
        with open(path, 'rb') as f:
            return f.read().decode('utf-8', 'replace').replace('\r', '')
    except OSError:
        return None


def parse_time(text):
    """'24.707' -> (24.707, 0.001); '9m10s' -> (550.0, 1.0); '1h02m' -> (3720.0, 60.0).
    The second number is the resolution the unit printed.  None: not a time."""
    if re.match(r'^\d+\.\d+$', text):
        return float(text), 0.001
    m = re.match(r'^(?:(\d+)d)?(?:(\d+)h)?(?:(\d+)m)?(?:(\d+)s)?$', text)
    if not m or not any(g is not None for g in m.groups()):
        return None
    total, res = 0.0, None
    for value, (_, size) in zip(m.groups(), UNITS):
        if value is not None:
            total += int(value) * size
            res = size
    return total, res


def parse_times(text):
    """{pid: {'name', 'own', 'kids', 'res'}} from one "pidin times" snapshot."""
    out = {}
    for line in (text or '').split('\n'):
        tok = line.split()
        if len(tok) < 9 or not tok[0].isdigit() or not tok[-8].isdigit():
            continue
        times = [parse_time(t) for t in tok[-4:]]
        if any(t is None for t in times):
            continue
        out[int(tok[0])] = {
            'name': ' '.join(tok[1:-8]),
            'own': times[0][0] + times[1][0],
            'kids': times[2][0] + times[3][0],
            'res': max(times[0][1], times[1][1]),
        }
    return out


def parse_ttimes(text):
    """{(pid, tid): {'name', 'time', 'res', 'late'}} from one "pidin ttimes" snapshot.
    late: the thread started in a later minute than its process."""
    out = {}
    for line in (text or '').split('\n'):
        tok = line.split()
        if len(tok) < 11 or not tok[0].isdigit() or not tok[1].isdigit():
            continue
        t = parse_time(tok[-1])
        if t is None or not re.match(r'^\d\d:\d\d$', tok[-2]) or not re.match(r'^\d\d:\d\d$', tok[-5]):
            continue
        out[(int(tok[0]), int(tok[1]))] = {
            'name': ' '.join(tok[2:-8]),
            'time': t[0],
            'res': t[1],
            'late': tok[-7:-4] != tok[-4:-1],
            'born': ' '.join(tok[-4:-1]),
        }
    return out


def threads(a, b, args):
    """Threads that ran between two "pidin ttimes" snapshots, busiest first, without
    procnto (kernel + idle): [{'sec', 'res', 'label', 'pid', 'tid', 'late', 'born', 'total'}]."""
    rows = []
    for key, y in b.items():
        x = a.get(key)
        if x is None or key[0] == 1 or y['time'] <= x['time']:
            continue
        rows.append({'sec': y['time'] - x['time'], 'res': max(x['res'], y['res']),
                     'label': label(key[0], y['name'], args), 'pid': key[0], 'tid': key[1],
                     'late': y['late'], 'born': y['born'], 'total': y['time']})
    rows.sort(key=lambda r: (-r['sec'], r['pid'], r['tid']))
    return rows


def probe_thread(rows, window):
    """The thread that is the measurement itself, or None: a j9 thread started after
    its process, busy for at least 90 % of the window, with under a minute of work
    before it (it began with the script, not long ago)."""
    for r in rows:
        if (r['label'].startswith('j9') and r['late'] and r['sec'] >= 0.9 * window
                and r['total'] - r['sec'] < 60.0):
            return r
    return None


def parse_args(text):
    """pid -> command line, from pidin-ar.txt."""
    out = {}
    for line in (text or '').split('\n'):
        m = re.match(r'^\s*(\d+)\s+(\S.*)$', line)
        if m:
            out[int(m.group(1))] = m.group(2).strip()
    return out


def label(pid, name, args):
    """A name a person recognises: the script behind 'sh', the binary without its path."""
    cmd = args.get(pid, '')
    words = cmd.split()
    if words:
        base = os.path.basename(words[0])
        if base in ('sh', 'ksh') and len(words) > 1:
            return os.path.basename(words[1])
        if base == 'j9':
            m = re.search(r'myProcName=(\S+)', cmd)
            return 'j9 (%s)' % m.group(1) if m else 'j9'
        return base
    return os.path.basename(name) if name else str(pid)


def whose(name):
    if any(o in name for o in OURS):
        return 'nasz'
    if any(h in name for h in HOSTS):
        return 'fabryczny z naszym kodem'
    return ''


def cores_from_info(text):
    n = len(re.findall(r'(?im)^\s*Processor\s*\d+\s*:', text or ''))
    return n or None


def free_mem_from_info(text):
    m = re.search(r'(?i)FreeMem\s*:\s*(\S+?)\s*/\s*(\S+)', text or '')
    return (m.group(1), m.group(2)) if m else None


def measure(t1, t2, ncpu, args):
    """Rows by label: {'own', 'kids', 'res'} in seconds over the window, the
    window itself, and procnto's share (kernel + idle).  None without cores."""
    rows, total, kernel_idle = {}, 0.0, 0.0
    for pid, b in t2.items():
        a = t1.get(pid)
        if a is None:
            continue
        own, kids = b['own'] - a['own'], b['kids'] - a['kids']
        if own < 0 or kids < 0:
            continue
        total += own + kids
        if pid == 1:
            kernel_idle += own
            continue
        row = rows.setdefault(label(pid, b['name'], args), {'own': 0.0, 'kids': 0.0, 'res': 0.001})
        row['own'] += own
        row['kids'] += kids
        row['res'] = max(row['res'], a['res'], b['res'])
    if not ncpu or total <= 0:
        return None
    return {'rows': rows, 'window': total / ncpu, 'kernel_idle': kernel_idle, 'ncpu': ncpu}


def load(run):
    """(measure() result or None, free memory, status, problems) for one run."""
    cpu = os.path.join(run, 'cpu')
    notes = []
    if not os.path.isdir(cpu):
        return None, None, {}, ['BRAK katalogu cpu/ w %s: przebieg zebrany wersja sprzed pomiaru (przed 2.0.0-rc4).' % run]
    st = {}
    for line in (read(os.path.join(cpu, 'status.txt')) or '').split('\n'):
        if '=' in line:
            k, v = line.split('=', 1)
            st[k.strip()] = v.strip()
    info = read(os.path.join(cpu, 'pidin-info.txt'))
    t1 = parse_times(read(os.path.join(cpu, 'pidin-times-1.txt')))
    t2 = parse_times(read(os.path.join(cpu, 'pidin-times-2.txt')))
    m = None
    if t1 and t2:
        args = parse_args(read(os.path.join(run, 'pidin-ar.txt')))
        m = measure(t1, t2, cores_from_info(info), args)
        if m is None:
            notes.append('Nie da sie policzyc okna: brak liczby rdzeni w pidin-info.txt.')
        else:
            raw1 = read(os.path.join(cpu, 'pidin-ttimes-1.txt'))
            h1 = parse_ttimes(raw1)
            h2 = parse_ttimes(read(os.path.join(cpu, 'pidin-ttimes-2.txt')))
            m['threads'] = threads(h1, h2, args) if h1 and h2 else None
            m['probe'] = probe_thread(m['threads'], m['window']) if m['threads'] else None
            if raw1 is not None and m['threads'] is None:
                notes.append('pidin ttimes: nieznany format, bez czasow na watek; poczatek:')
                notes.extend('  | ' + line[:110] for line in raw1.split('\n')[:4])
    else:
        notes.append('pidin times: brak danych albo nieznany format (1=%s, 2=%s)' % (
            st.get('PIDIN_TIMES_1', '?'), st.get('PIDIN_TIMES_2', '?')))
        raw = read(os.path.join(cpu, 'pidin-times-1.txt'))
        if raw:
            notes.extend('  | ' + line for line in raw.split('\n')[:6])
    return m, free_mem_from_info(info), st, notes


def percent(seconds, window):
    return 100.0 * seconds / window


def busy(m):
    """Load of the unit without procnto (kernel + idle), percent of all cores."""
    used = sum(r['own'] + r['kids'] for r in m['rows'].values())
    return percent(used, m['window'] * m['ncpu'])


def probe_percent(m):
    """What the measurement thread took, percent of one core; never above a core
    (its snapshots span a little more than the window)."""
    return min(100.0, percent(m['probe']['sec'], m['window']))


def busy_without_probe(m):
    """Load of the unit, percent of ONE core summed over the cores, without the
    measurement thread.  None when there are no thread times or no such thread."""
    if not m.get('probe'):
        return None
    return busy(m) * m['ncpu'] - probe_percent(m)


def shmem_files(ls_text):
    """[(size, name)] of the files in /dev/shmem, biggest first."""
    files = []
    for line in (ls_text or '').split('\n'):
        tok = line.split()
        if len(tok) >= 8 and tok[4].isdigit() and not line.startswith('d'):
            files.append((int(tok[4]), tok[-1]))
    files.sort(reverse=True)
    return files


def report(run, other=None):
    m, mem, st, notes = load(run)
    out = ['Przebieg: %s' % run]
    if st:
        out.append('Kroki pomiaru: ' + ', '.join('%s=%s' % kv for kv in sorted(st.items())))
    out.extend(notes)
    if m is None:
        out.append('NIEZNANY FORMAT albo brak pomiaru: nie ma z czego liczyc.')
        return 2, out

    mo = None
    if other:
        mo, memo, _, noteso = load(other)
        out.append('Porownanie z: %s' % other)
        out.extend(noteso)
        if mo is None:
            out.append('Drugi przebieg bez pomiaru: pokazuje tylko pierwszy.')

    out.append('')
    out.append('Okno: %.1f s, rdzeni: %d' % (m['window'], m['ncpu']))
    line = 'Obciazenie jednostki bez jadra i bezczynnosci: %.0f%% z %d%%' % (
        busy(m) * m['ncpu'], 100 * m['ncpu'])
    if mo:
        line += '   (bez CarPlay: %.0f%%)' % (busy(mo) * mo['ncpu'])
    out.append(line)
    if m.get('probe'):
        line = 'W tym sam pomiar (watek zielonego menu czeka na skrypt): %.0f%%, bez niego: %.0f%% z %d%%' % (
            probe_percent(m), busy_without_probe(m), 100 * m['ncpu'])
        if mo:
            line += ('   (bez CarPlay: %.0f%%)' % busy_without_probe(mo) if mo.get('probe')
                     else '   (bez CarPlay: brak czasow na watek)')
        out.append(line)
    if mem:
        out.append('Wolna pamiec: %s z %s%s' % (mem[0], mem[1],
                   '   (bez CarPlay: %s)' % memo[0] if mo and memo else ''))

    out.append('')
    out.append('Procent jednego rdzenia; "dzieci" = zakonczone polecenia uruchomione przez proces:')
    if mo:
        out.append('%8s %8s %8s %8s  %s' % ('proces', 'dzieci', 'bez', 'roznica', ''))
    else:
        out.append('%8s %8s  %s' % ('proces', 'dzieci', ''))
    rows = []
    names = set(m['rows']) | (set(mo['rows']) if mo else set())
    for name in names:
        r = m['rows'].get(name, {'own': 0.0, 'kids': 0.0, 'res': 0.001})
        own, kids = percent(r['own'], m['window']), percent(r['kids'], m['window'])
        base = None
        if mo:
            ro = mo['rows'].get(name, {'own': 0.0, 'kids': 0.0})
            base = percent(ro['own'] + ro['kids'], mo['window'])
        rows.append((own + kids, own, kids, base, name, r['res']))
    rows.sort(key=lambda r: (-(r[0] - r[3]) if mo else -r[0], r[4]))
    shown = 0
    for both, own, kids, base, name, res in rows:
        mark = whose(name)
        big = both >= 1.0 or (base is not None and abs(both - base) >= 1.0)
        if not (big or mark == 'nasz' or shown < 12):
            continue
        shown += 1
        tail = name
        if mark:
            tail += '   <- ' + mark
        if res >= 1.0:
            tail += '   (+-%.0f%%: czas w calych sekundach)' % percent(res, m['window'])
        if m.get('probe') and name == m['probe']['label']:
            tail += '   (w tym %.0f%% to sam pomiar)' % probe_percent(m)
        if mo:
            out.append('%7.1f%% %7.1f%% %7.1f%% %+7.1f%%  %s' % (own, kids, base, both - base, tail))
        else:
            out.append('%7.1f%% %7.1f%%  %s' % (own, kids, tail))
    ours = sum(r['own'] + r['kids'] for n, r in m['rows'].items() if whose(n) == 'nasz')
    out.append('Nasze procesy razem: %.1f%% jednego rdzenia' % percent(ours, m['window']))

    if m.get('threads'):
        out.append('')
        out.append('Najciezsze watki (procent jednego rdzenia; ich migawki obejmuja okno dluzsze o ok. sekunde):')
        for r in m['threads'][:8]:
            tail = '%s watek %d' % (r['label'], r['tid'])
            if r['late']:
                tail += ', powstal %s' % r['born']
            if r is m['probe']:
                tail += '   <- sam pomiar: zielone menu czeka na skrypt'
            elif r['res'] >= 1.0:
                tail += '   (+-%.0f%%: czas w calych sekundach)' % percent(r['res'], m['window'])
            out.append('%7.1f%%  %s' % (percent(r['sec'], m['window']), tail))

    cpu = os.path.join(run, 'cpu')
    files = shmem_files(read(os.path.join(cpu, 'ls-shmem.txt')))
    if files:
        out.append('')
        out.append('Pliki w /dev/shmem: %d, razem %.1f MB; najwieksze:' % (len(files), sum(f[0] for f in files) / 1e6))
        for size, name in files[:6]:
            out.append('  %9d B  %s' % (size, name))
    return 0, out


def main(argv):
    args = argv[1:]
    other = None
    if '--bez' in args:
        i = args.index('--bez')
        if i + 1 >= len(args):
            sys.stderr.write(__doc__)
            return 1
        other = args[i + 1]
        del args[i:i + 2]
    if len(args) != 1:
        sys.stderr.write(__doc__)
        return 1
    code, lines = report(args[0], other)
    print('\n'.join(lines))
    return code


if __name__ == '__main__':
    sys.exit(main(sys.argv))
