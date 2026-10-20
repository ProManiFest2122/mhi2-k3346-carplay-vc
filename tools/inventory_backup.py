#!/usr/bin/env python3
"""Generate markdown inventory tables of the M.I.B. backup (read-only).

Reads the backup folder and the read-only extraction of app.img in work/,
and writes markdown tables to stdout. The unit's FAZIT serial is masked.

usage: inventory_backup.py BACKUP_ROOT WORK_DIR > tables.md
"""
import hashlib
import os
import re
import sys

backup, work = sys.argv[1], sys.argv[2]
FAZIT = re.compile(r"[A-Z]{3}-\d{5}\.\d{2}\.\d{10}")
NAME_PAT = re.compile(r"(?i)airplay|carplay|iap|ipod|dio_manager|smartphone_integrator|displaymanager|dmdt|mlb|most|kombi|komo|cluster|videoovermost|nvss|\.jxe$|dsitracer|navactiveignore")


def mask(p):
    return FAZIT.sub("<FAZIT>", p.replace("\\", "/"))


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def size_of_tree(root):
    tot = n = 0
    for r, _, fs in os.walk(root):
        for f in fs:
            tot += os.path.getsize(os.path.join(r, f))
            n += 1
    return tot, n


def human(n):
    for u in ("B", "KB", "MB", "GB"):
        if n < 1024:
            return "%.1f %s" % (n, u)
        n /= 1024.0
    return "%.1f TB" % n


out = []
tot, n = size_of_tree(backup)
out.append("### Rozmiar\n\n`backup_do_edycji/`: **%s** w **%d** plikach.\n" % (human(tot), n))

# tree depth 4 with sizes
out.append("### Drzewo katalogów (do głębokości 4, z rozmiarem poddrzew)\n\n```text")
base_depth = backup.rstrip("/\\").count(os.sep) + backup.rstrip("/\\").count("/")
for r, ds, fs in os.walk(backup):
    ds.sort()
    rel = os.path.relpath(r, backup)
    depth = 0 if rel == "." else rel.replace("\\", "/").count("/") + 1
    if depth > 4:
        ds[:] = []
        continue
    s, c = size_of_tree(r)
    name = "backup_do_edycji/" if rel == "." else os.path.basename(r) + "/"
    extra = "" if depth < 4 or not ds else "  (…)"
    out.append("%s%s  [%s, %d plików]%s" % ("  " * depth, mask(name), human(s), c, extra))
    if depth == 4:
        ds[:] = []
out.append("```\n")

# top-level files with hashes
mu = [d for d in os.listdir(backup) if d.startswith("MU")][0]
mudir = os.path.join(backup, mu)
out.append("### Pliki w katalogu głównym backupu (`%s/`)\n" % mask(mu))
out.append("| Plik | Rozmiar [B] | SHA-256 |\n|---|---:|---|")
for f in sorted(os.listdir(mudir)):
    p = os.path.join(mudir, f)
    if os.path.isfile(p):
        out.append("| `%s` | %d | `%s` |" % (mask(f), os.path.getsize(p), sha(p)))
out.append("")

out.append("### Obrazy partycji (advanced/MU1438-FW-images)\n")
out.append("| Ścieżka | Rozmiar [B] | SHA-256 |\n|---|---:|---|")
fw = os.path.join(mudir, "advanced", "MU1438-FW-images")
for r, _, fs in os.walk(fw):
    for f in sorted(fs):
        p = os.path.join(r, f)
        if f.endswith((".img", ".ifs", ".efs", ".bin", ".bgz")) and ("MMX2" in r or "RCC" in r):
            out.append("| `%s` | %d | `%s` |" % (mask(os.path.relpath(p, backup)), os.path.getsize(p), sha(p)))
out.append("")


def table_for(root, prefix, pred, title):
    out.append(title)
    out.append("| Ścieżka na jednostce | Rozmiar [B] | SHA-256 |\n|---|---:|---|")
    rows = []
    for r, _, fs in os.walk(root):
        for f in fs:
            p = os.path.join(r, f)
            rel = "/" + os.path.relpath(p, root).replace("\\", "/")
            if pred(rel, f) and not os.path.islink(p):
                rows.append((prefix + rel, os.path.getsize(p), sha(p)))
    for a, b, c in sorted(rows):
        out.append("| `%s` | %d | `%s` |" % (a, b, c))
    out.append("")
    return len(rows)


app = os.path.join(work, "mmx_app")


def app_pred(rel, f):
    if rel.startswith("/navigation/resources") or rel.startswith("/eso/browser"):
        return False
    if NAME_PAT.search(f):
        return True
    if rel.startswith("/eso/hmi/lsd/") and (f.endswith((".jar", ".jxe", ".properties", ".sh")) or "/jars/" in rel):
        return True
    if rel in ("/armle/usr/sbin/mm-ipod", "/armle/lib/dll/ipod-drvr-iap2.so", "/eso/bin/runLSD.sh",
               "/eso/bin/runHMI.sh", "/eso/bin/runMMX.sh", "/eso/bin/java.sh", "/armle/bin/usblauncher"):
        return True
    if re.match(r"/eso/bin/run.*\.sh$", rel):
        return True
    if rel.startswith("/eso/hmi/engdefs/scripts/") and re.search(r"(?i)carplay|dsitracer|displaymanager|ipod|navi", f):
        return True
    if rel.startswith("/eso/lib/factories/") and re.search(r"(?i)carplay|komo|kombi|carkombi|keypanel|bap|map|parking", f):
        return True
    return False


n1 = table_for(app, "/mnt/app", app_pred,
               "### Pliki istotne z `app.img` (partycja `/mnt/app` MMX, wyodrębnione do `work/mmx_app/`)\n")
sysd = os.path.join(work, "system")


def sys_pred(rel, f):
    if rel.startswith("/eso/production/") and f.endswith((".json", ".cfg")):
        return True
    if NAME_PAT.search(f) or f.startswith("usblauncher") or rel in ("/mm/iap2.cfg", "/ipod.cfg", "/boot/startup.sh", "/boot/boot.sh"):
        return True
    if rel.startswith("/scripts/") and re.search(r"(?i)carplay|ipod|otg|ncm", f):
        return True
    return False


n2 = table_for(sysd, "/etc", sys_pred,
               "### Konfiguracja z `advanced/MU1438-system` (= `/etc` MMX; kopia w `work/system/`)\n")
sys.stdout.write("\n".join(out) + "\n")
sys.stderr.write("app rows=%d sys rows=%d\n" % (n1, n2))
