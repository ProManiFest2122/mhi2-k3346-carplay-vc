#!/usr/bin/env python3
"""Read-only QNX IFS (mkifs image) extractor with pure-Python LZO1X/zlib.

Handles images that begin with a startup_header (signature 0x00ff7eeb),
optionally preceded by boot code, whose image part is compressed in
blocks of [u16 big-endian length][payload] terminated by length 0.

usage: qnx_ifs_extract.py IMAGE OUTDIR
"""
import os
import stat
import struct
import sys
import zlib

STARTUP_SIG = 0x00FF7EEB


def lzo1x_decompress(src):
    """LZO1X decompression (port of Linux lib/lzo/lzo1x_decompress_safe.c)."""
    out = bytearray()
    ip = 0
    n = len(src)
    state = 0

    def copy_match(m_pos, length):
        if m_pos < 0:
            raise ValueError("lzo: lookbehind overrun")
        for _ in range(length):
            out.append(out[m_pos])
            m_pos += 1

    t = src[ip]
    nxt = None
    if t > 17:
        ip += 1
        t -= 17
        if t < 4:
            nxt = t
        else:
            out += src[ip:ip + t]
            ip += t
            state = 4
    while True:
        if nxt is None:
            t = src[ip]
            ip += 1
            if t < 16:
                if state == 0:
                    if t == 0:
                        while src[ip] == 0:
                            t += 255
                            ip += 1
                        t += 15 + src[ip]
                        ip += 1
                    t += 3
                    out += src[ip:ip + t]
                    ip += t
                    state = 4
                    continue
                elif state != 4:
                    nxt = t & 3
                    m_pos = len(out) - 1 - (t >> 2) - (src[ip] << 2)
                    ip += 1
                    copy_match(m_pos, 2)
                else:
                    nxt = t & 3
                    m_pos = len(out) - (1 + 0x0800) - (t >> 2) - (src[ip] << 2)
                    ip += 1
                    copy_match(m_pos, 3)
            elif t >= 64:
                nxt = t & 3
                m_pos = len(out) - 1 - ((t >> 2) & 7) - (src[ip] << 3)
                ip += 1
                copy_match(m_pos, (t >> 5) + 1)
            elif t >= 32:
                t = (t & 31) + 2
                if t == 2:
                    while src[ip] == 0:
                        t += 255
                        ip += 1
                    t += 31 + src[ip]
                    ip += 1
                v = src[ip] | (src[ip + 1] << 8)
                ip += 2
                m_pos = len(out) - 1 - (v >> 2)
                nxt = v & 3
                copy_match(m_pos, t)
            else:  # 16..31
                m_pos = len(out) - ((t & 8) << 11)
                t = (t & 7) + 2
                if t == 2:
                    while src[ip] == 0:
                        t += 255
                        ip += 1
                    t += 7 + src[ip]
                    ip += 1
                v = src[ip] | (src[ip + 1] << 8)
                ip += 2
                m_pos -= v >> 2
                nxt = v & 3
                if m_pos == len(out):
                    return bytes(out)  # end-of-stream marker
                m_pos -= 0x4000
                copy_match(m_pos, t)
        # match_next: copy 0..3 trailing literals
        state = nxt
        out += src[ip:ip + nxt]
        ip += nxt
        nxt = None
        if ip >= n:
            return bytes(out)


def find_startup(data):
    for off in range(0, min(len(data), 0x1000), 4):
        if struct.unpack_from("<I", data, off)[0] == STARTUP_SIG:
            return off
    raise ValueError("startup header not found")


def decompress_image(data, hoff):
    flags1 = data[hoff + 6]
    startup_size = struct.unpack_from("<I", data, hoff + 32)[0]
    stored_size = struct.unpack_from("<I", data, hoff + 36)[0]
    imagefs_size = struct.unpack_from("<I", data, hoff + 44)[0]
    comp = flags1 & 0x1C
    print("startup@0x%x flags1=0x%02x startup_size=0x%x stored=0x%x imagefs=0x%x comp=%s" % (
        hoff, flags1, startup_size, stored_size, imagefs_size, {0: "none", 4: "zlib", 8: "lzo", 12: "ucl"}.get(comp)))
    p = hoff + startup_size
    if comp == 0:
        return data[p:p + imagefs_size]
    out = bytearray()
    blocks = 0
    while p + 2 <= len(data):
        ln = (data[p] << 8) | data[p + 1]
        p += 2
        if ln == 0:
            break
        chunk = data[p:p + ln]
        p += ln
        if comp == 8:
            out += lzo1x_decompress(chunk)
        elif comp == 4:
            out += zlib.decompress(chunk)
        else:
            raise ValueError("UCL compression not implemented")
        blocks += 1
    print("decompressed %d blocks -> %d bytes" % (blocks, len(out)))
    return bytes(out)


def parse_imagefs(img, outdir):
    off = img.find(b"imagefs")
    if off < 0:
        raise ValueError("imagefs signature not found")
    base = off
    image_size, hdr_dir_size, dir_offset = struct.unpack_from("<III", img, base + 8)
    mp = img[base + 0x5c:img.index(b"\0", base + 0x5c)].decode("latin-1")
    print("imagefs@0x%x size=0x%x dir_size=0x%x dir_off=0x%x mountpoint=%r" % (base, image_size, hdr_dir_size, dir_offset, mp))
    p = base + dir_offset
    listing = []
    while True:
        size, extoff = struct.unpack_from("<HH", img, p)
        if size == 0:
            break
        ino, mode, gid, uid, mtime = struct.unpack_from("<IIIII", img, p + 4)
        body = p + 24
        if stat.S_ISREG(mode):
            foff, fsize = struct.unpack_from("<II", img, body)
            name = img[body + 8:img.index(b"\0", body + 8)].decode("latin-1")
            dest = os.path.join(outdir, name.replace("/", os.sep))
            os.makedirs(os.path.dirname(dest) or outdir, exist_ok=True)
            with open(dest, "wb") as fo:
                fo.write(img[base + foff:base + foff + fsize])
            listing.append("f %o %10d %s" % (mode & 0o7777, fsize, name))
        elif stat.S_ISDIR(mode):
            name = img[body:img.index(b"\0", body)].decode("latin-1")
            os.makedirs(os.path.join(outdir, name.replace("/", os.sep)), exist_ok=True)
            listing.append("d %o %10d %s" % (mode & 0o7777, 0, name))
        elif stat.S_ISLNK(mode):
            sym_off, sym_size = struct.unpack_from("<HH", img, body)
            name = img[body + 4:img.index(b"\0", body + 4)].decode("latin-1")
            target = img[body + sym_off:body + sym_off + sym_size].decode("latin-1")
            listing.append("l %o %10d %s -> %s" % (mode & 0o7777, 0, name, target))
        else:
            name = img[body + 8:img.index(b"\0", body + 8)].decode("latin-1", "replace")
            listing.append("? %o %10d %s" % (mode, 0, name))
        p += size
    return listing


def main():
    src, outdir = sys.argv[1], sys.argv[2]
    data = open(src, "rb").read()
    hoff = find_startup(data)
    img = decompress_image(data, hoff)
    os.makedirs(outdir, exist_ok=True)
    listing = parse_imagefs(img, outdir)
    with open(outdir.rstrip("/\\") + "_listing.txt", "w", encoding="utf-8") as fo:
        fo.write("\n".join(listing) + "\n")
    print("entries=%d" % len(listing))


if __name__ == "__main__":
    main()
