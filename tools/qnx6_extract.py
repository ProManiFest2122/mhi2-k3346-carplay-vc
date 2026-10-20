#!/usr/bin/env python3
"""Read-only QNX6 (Power-Safe) filesystem extractor.

Opens the image strictly read-only and copies the file tree to an output
directory. Format per Linux fs/qnx6 (qnx6_fs.h / inode.c / dir.c).

usage: qnx6_extract.py IMAGE OUTDIR [--list]
"""
import mmap
import os
import stat
import struct
import sys

BOOTBLOCK = 0x2000
SB_AREA = 0x1000
MAGIC = 0x68191122


class QNX6:
    def __init__(self, path):
        self.f = open(path, "rb")
        self.m = mmap.mmap(self.f.fileno(), 0, access=mmap.ACCESS_READ)
        sb = self.m[BOOTBLOCK:BOOTBLOCK + 512]
        magic = struct.unpack_from("<I", sb, 0)[0]
        if magic == MAGIC:
            self.e = "<"
        elif struct.unpack_from(">I", sb, 0)[0] == MAGIC:
            self.e = ">"
        else:
            raise ValueError("not a QNX6 image (magic 0x%08x)" % magic)
        self.blocksize = self.u32(sb, 48)
        self.num_inodes = self.u32(sb, 52)
        self.num_blocks = self.u32(sb, 60)
        self.serial = struct.unpack_from(self.e + "Q", sb, 8)[0]
        self.blks_off = BOOTBLOCK + SB_AREA
        # pick the newer of the two superblocks (second lies after the data area)
        sb2_off = self.blks_off + self.num_blocks * self.blocksize
        if sb2_off + 512 <= len(self.m):
            sb2 = self.m[sb2_off:sb2_off + 512]
            if struct.unpack_from(self.e + "I", sb2, 0)[0] == MAGIC:
                serial2 = struct.unpack_from(self.e + "Q", sb2, 8)[0]
                if serial2 > self.serial:
                    sb = sb2
                    self.serial = serial2
        self.sb = sb
        self.inode_root = self.root_node(sb, 72)
        self.longfile_root = self.root_node(sb, 72 + 80 * 2)
        self.ptrs_per_block = self.blocksize // 4

    def u32(self, buf, off):
        return struct.unpack_from(self.e + "I", buf, off)[0]

    def root_node(self, sb, off):
        size = struct.unpack_from(self.e + "Q", sb, off)[0]
        ptrs = list(struct.unpack_from(self.e + "16I", sb, off + 8))
        levels = sb[off + 72]
        return size, ptrs, levels

    def block(self, n):
        o = self.blks_off + n * self.blocksize
        return self.m[o:o + self.blocksize]

    def _leaves(self, ptrs, levels):
        """Yield data block numbers in logical order."""
        for p in ptrs:
            if p == 0xFFFFFFFF:
                continue
            if levels == 0:
                yield p
            else:
                sub = struct.unpack_from(self.e + "%dI" % self.ptrs_per_block, self.block(p))
                for q in self._leaves(sub, levels - 1):
                    yield q

    def read_tree(self, size, ptrs, levels):
        out = bytearray()
        for b in self._leaves(ptrs, levels):
            out += self.block(b)
            if len(out) >= size:
                break
        return bytes(out[:size])

    def logical_block(self, ptrs, levels, idx):
        # direct computation of the idx-th leaf
        span = self.ptrs_per_block ** levels
        top = idx // span
        p = ptrs[top]
        rem = idx % span
        for lvl in range(levels, 0, -1):
            span //= self.ptrs_per_block
            tbl = struct.unpack_from(self.e + "%dI" % self.ptrs_per_block, self.block(p))
            p = tbl[rem // span]
            rem %= span
        return self.block(p)

    def inode(self, ino):
        size, ptrs, levels = self.inode_root
        off = (ino - 1) * 128
        blk = self.logical_block(ptrs, levels, off // self.blocksize)
        raw = blk[off % self.blocksize: off % self.blocksize + 128]
        e = self.e
        return {
            "size": struct.unpack_from(e + "Q", raw, 0)[0],
            "uid": self.u32(raw, 8), "gid": self.u32(raw, 12),
            "mtime": self.u32(raw, 20),
            "mode": struct.unpack_from(e + "H", raw, 32)[0],
            "ptrs": list(struct.unpack_from(e + "16I", raw, 36)),
            "levels": raw[100], "status": raw[101],
        }

    def data(self, ino_rec):
        return self.read_tree(ino_rec["size"], ino_rec["ptrs"], ino_rec["levels"])

    def longname(self, idx):
        size, ptrs, levels = self.longfile_root
        blk = self.logical_block(ptrs, levels, idx)
        n = struct.unpack_from(self.e + "H", blk, 0)[0]
        return blk[2:2 + n].decode("latin-1")

    def listdir(self, ino_rec):
        d = self.data(ino_rec)
        for i in range(0, len(d) - 31, 32):
            ino = self.u32(d, i)
            if ino == 0:
                continue
            ln = d[i + 4]
            if ln <= 27:
                name = d[i + 5:i + 5 + ln].decode("latin-1")
            else:
                name = self.longname(self.u32(d, i + 8))
            if name in (".", ".."):
                continue
            yield name, ino

    def walk(self, ino=1, path=""):
        rec = self.inode(ino)
        for name, child in self.listdir(rec):
            crec = self.inode(child)
            p = path + "/" + name
            yield p, child, crec
            if stat.S_ISDIR(crec["mode"]):
                yield from self.walk(child, p)


def main():
    img, out = sys.argv[1], sys.argv[2]
    list_only = "--list" in sys.argv
    fs = QNX6(img)
    print("blocksize=%d inodes=%d blocks=%d serial=%d" % (fs.blocksize, fs.num_inodes, fs.num_blocks, fs.serial))
    n_files = n_dirs = n_links = 0
    total = 0
    listing = []
    for p, ino, rec in fs.walk():
        mode = rec["mode"]
        dest = os.path.join(out, p.lstrip("/").replace("/", os.sep))
        if stat.S_ISDIR(mode):
            n_dirs += 1
            listing.append("d %o %10d %s" % (mode & 0o7777, 0, p))
            if not list_only:
                os.makedirs(dest, exist_ok=True)
        elif stat.S_ISLNK(mode):
            n_links += 1
            target = fs.data(rec).decode("latin-1")
            listing.append("l %o %10d %s -> %s" % (mode & 0o7777, rec["size"], p, target))
        elif stat.S_ISREG(mode):
            n_files += 1
            total += rec["size"]
            listing.append("f %o %10d %s" % (mode & 0o7777, rec["size"], p))
            if not list_only:
                os.makedirs(os.path.dirname(dest), exist_ok=True)
                with open(dest, "wb") as fo:
                    fo.write(fs.data(rec))
        else:
            listing.append("? %o %10d %s" % (mode, rec["size"], p))
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "..", os.path.basename(out.rstrip("/\\")) + "_listing.txt"), "w", encoding="utf-8") as fo:
        fo.write("\n".join(listing) + "\n")
    print("dirs=%d files=%d links=%d bytes=%d" % (n_dirs, n_files, n_links, total))


if __name__ == "__main__":
    main()
