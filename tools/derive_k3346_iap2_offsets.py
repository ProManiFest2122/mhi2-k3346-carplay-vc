#!/usr/bin/env python3
"""Derive K3346 equivalents of edzenis' K2161 ipod-drvr-iap2.so constants.

Read-only static analysis using the driver's own .symtab/.dynsym, relocation
table and table structure. Prints a candidate offset profile plus the
validation evidence for each value. Nothing here is patched or written to
the firmware; output is a research table for porting the C sources.

usage: derive_k3346_iap2_offsets.py ipod-drvr-iap2.so
"""
import struct
import sys

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

R_ARM_RELATIVE = 23
R_ARM_ABS32 = 2
R_ARM_JUMP_SLOT = 22


def main(path):
    raw = open(path, "rb").read()
    elf = ELFFile(open(path, "rb"))

    def va2off(va):
        for seg in elf.iter_segments():
            if seg["p_type"] == "PT_LOAD" and seg["p_vaddr"] <= va < seg["p_vaddr"] + seg["p_filesz"]:
                return va - seg["p_vaddr"] + seg["p_offset"]
        return None

    def u32(va):
        o = va2off(va)
        return None if o is None else struct.unpack_from("<I", raw, o)[0]

    def u16(va):
        o = va2off(va)
        return None if o is None else struct.unpack_from("<H", raw, o)[0]

    def cstr(va):
        o = va2off(va)
        if o is None:
            return None
        return raw[o:raw.index(b"\0", o)].decode("latin-1")

    syms = {}
    funcs = []
    for secname in (".symtab", ".dynsym"):
        sec = elf.get_section_by_name(secname)
        for s in sec.iter_symbols():
            if s.name and s["st_value"]:
                syms.setdefault(s.name, (s["st_value"], s["st_size"], s["st_info"]["type"]))
    for n, (v, sz, t) in syms.items():
        if t == "STT_FUNC":
            funcs.append((v & ~1, sz, n))
    funcs.sort()

    def func_at(va):
        for v, sz, n in funcs:
            if v <= va < v + max(sz, 1):
                return n, v
        return None, None

    relocs = {}
    jump_slots = []
    for sec in elf.iter_sections():
        if isinstance(sec, RelocationSection):
            symtab = elf.get_section(sec["sh_link"])
            for r in sec.iter_relocations():
                name = symtab.get_symbol(r["r_info_sym"]).name if r["r_info_sym"] else None
                relocs[r["r_offset"]] = (r["r_info_type"], name)
                if sec.name == ".rel.plt":
                    jump_slots.append(name)

    plt = elf.get_section_by_name(".plt")
    plt_addr = {}
    for i, name in enumerate(jump_slots):
        plt_addr[plt["sh_addr"] + 20 + 12 * i] = name
    plt_by_name = {v: k for k, v in plt_addr.items()}

    out = []

    def rec(key, k2161, k3346, evidence):
        out.append((key, k2161, k3346, evidence))

    # ---- exported anchors (observer.c) -------------------------------------
    rec("ipod_module (dlsym anchor)", 0x2614C, syms["ipod_module"][0], "dynsym OBJECT")
    rec("packet_addparam_blob (dlsym anchor)", 0x1D0D8, syms["packet_addparam_blob"][0], "dynsym FUNC")

    # ---- functions (send_hook.c) -------------------------------------------
    for key, k2161, name in [
        ("IAP2_MSG_OFFSET", 0x0E484, "iap2_msg"),
        ("IAP2_CLOSE_OFFSET", 0x0F760, "iap2_ocb_close"),
        ("LINK_SEND_MESSAGE_OFFSET", 0x10530, "link_send_message"),
        ("ADDPARAM_NODATA_OFFSET", 0x1CF50, "packet_addparam_nodata"),
        ("ADDPARAM_NUM_OFFSET", 0x1D210, "packet_addparam_num"),
        ("SENDLIST_PUSH_OFFSET", 0x1DC14, "packet_sendlist_push"),
        ("SENDLIST_POP_OFFSET", 0x1DFAC, "packet_sendlist_pop"),
        ("K2161_ADDPARAM_BLOB_OFFSET", 0x1D0D8, "packet_addparam_blob"),
    ]:
        rec(key, k2161, syms[name][0], "symbol %s size=%d" % (name, syms[name][1]))

    # ---- resmgr/io function-pointer slots holding iap2_msg / iap2_ocb_close --
    want = {syms["iap2_msg"][0]: "IAP2_MSG_SLOT_OFFSET", syms["iap2_ocb_close"][0]: "IAP2_CLOSE_SLOT_OFFSET"}
    k2161_slots = {"IAP2_MSG_SLOT_OFFSET": 0xA7DAC, "IAP2_CLOSE_SLOT_OFFSET": 0xA7DC0}
    found = {}
    for sec in elf.iter_sections():
        if sec.name in (".data", ".data.rel.ro") and sec["sh_type"] == "SHT_PROGBITS":
            a0 = sec["sh_addr"]
            for a in range(a0, a0 + sec["sh_size"], 4):
                v = u32(a)
                if v in want:
                    found.setdefault(want[v], []).append((a, sec.name, relocs.get(a)))
    for key in ("IAP2_MSG_SLOT_OFFSET", "IAP2_CLOSE_SLOT_OFFSET"):
        cands = found.get(key, [])
        ev = "; ".join("0x%x(%s,reloc=%s)" % c for c in cands) or "NOT FOUND"
        rec(key, k2161_slots[key], cands[0][0] if len(cands) == 1 else None,
            "words == target fn: " + ev)
    # who owns those slots (nearest preceding data symbol)
    data_syms = sorted((v, sz, n) for n, (v, sz, t) in syms.items() if t == "STT_OBJECT")

    def owner(va):
        best = None
        for v, sz, n in data_syms:
            if v <= va:
                best = (v, sz, n)
        return best

    # ---- ident_info_funcs: {u32 id, fn*} entries; find id 0x16 -------------
    base_if, size_if, _ = syms["ident_info_funcs"]
    entries = []
    for a in range(base_if, base_if + size_if, 8):
        entries.append((a, u32(a), u32(a + 4)))
    gps = [e for e in entries if e[1] == 0x16]
    if gps:
        a, ident, fn = gps[0]
        fname, _ = func_at(fn)
        rec("K2161_IDENT_ID_OFFSET (value 0x0016)", 0xA7D80, a, "ident_info_funcs[%d] id=0x%x" % ((a - base_if) // 8, ident))
        rec("K2161_IDENT_BUILDER_OFFSET", 0xA7D84, a + 4, "slot -> 0x%x (%s), reloc=%s" % (fn, fname, relocs.get(a + 4)))
        rec("K2161_GPS_BUILDER_OFFSET", 0x0A11C, fn, "function %s" % fname)

    # ---- iap2_ctrl_msg_table ------------------------------------------------
    tbl = syms["iap2_ctrl_msg_table"][0]
    rec("K2161_CTRL_MSG_TABLE_OFFSET", 0xAB050, tbl, "symbol iap2_ctrl_msg_table size=%d (=%d groups x 12)" % (
        syms["iap2_ctrl_msg_table"][1], syms["iap2_ctrl_msg_table"][1] // 12))
    groups = {}
    for g in (0x50, 0x52, 0x68):
        a = tbl + 12 * g
        groups[g] = (u32(a), u32(a + 4), u32(a + 8))
    n50, m50, c50 = groups[0x50]
    n68, m68, c68 = groups[0x68]
    rec("K2161_GROUP50_NAME_OFFSET", 0x237DC, n50, "%r count=%d" % (cstr(n50), c50))
    rec("K2161_GROUP50_MESSAGES_OFFSET", 0xAC4DC, m50, "group 0x50 messages array")
    rec("K2161_GROUP68_NAME_OFFSET", 0x237E8, n68, "%r count=%d" % (cstr(n68), c68))
    rec("K2161_GROUP68_MESSAGES_OFFSET", 0xAC50C, m68, "group 0x68 messages array")
    np0 = (u32(m50), u32(m50 + 4), u32(m50 + 8))
    np1 = (u32(m50 + 12), u32(m50 + 16), u32(m50 + 20))
    rec("K2161_NP_START_NAME_OFFSET", 0x23B80, np0[0], "%r flags=%d params=0x%x" % (cstr(np0[0]), np0[1], np0[2]))
    rec("K2161_NP_START_PARAMS_OFFSET", 0x26BD8, np0[2], "== params_nowplay_start 0x%x" % syms["params_nowplay_start"][0])
    rec("K2161_NP_UPDATE_NAME_OFFSET", 0x23B98, np1[0], "%r flags=%d params=0x%x" % (cstr(np1[0]), np1[1], np1[2]))
    rec("K2161_NP_UPDATE_PARAMS_OFFSET", 0x26BF8, np1[2], "== params_nowplay_upd 0x%x" % syms["params_nowplay_upd"][0])
    rec("group 0x52 stock state (must be 0,0,0)", None, None, "name=0x%x msgs=0x%x count=%d" % groups[0x52])

    # ---- memcpy call sites (observer.c RX callsite 0x11428) -----------------
    memcpy_plt = plt_by_name.get("memcpy")
    text = elf.get_section_by_name(".text")
    sites = []
    for a in range(text["sh_addr"], text["sh_addr"] + text["sh_size"], 4):
        w = u32(a)
        if (w & 0x0F000000) == 0x0B000000 and (w >> 28) != 0xF:  # BL<cond>
            imm = w & 0xFFFFFF
            if imm & 0x800000:
                imm -= 0x1000000
            tgt = a + 8 + (imm << 2)
            if tgt == memcpy_plt:
                fn, fstart = func_at(a)
                sites.append((a + 4, fn, a + 4 - fstart if fstart is not None else None))
    print("memcpy@plt = %s" % (hex(memcpy_plt) if memcpy_plt else None))
    rx_fns = ("link_get_transport_data", "link_handle_iap2pkt", "transport_recv_pkt", "link_send_raw_message")
    rx = [s for s in sites if s[1] in rx_fns]
    rec("observer RX memcpy return VA", 0x11428, None,
        "candidates (ret, fn, +off): " + ", ".join("0x%x %s+0x%x" % s for s in rx))

    # ---- print ------------------------------------------------------------
    print("\n%-42s %-9s %-9s %s" % ("constant", "K2161", "K3346", "evidence"))
    for key, a, b, ev in out:
        print("%-42s %-9s %-9s %s" % (key, ("0x%05x" % a) if a is not None else "-",
                                       ("0x%05x" % b) if b is not None else "?", ev))
    for key in ("IAP2_MSG_SLOT_OFFSET", "IAP2_CLOSE_SLOT_OFFSET"):
        for a, secn, r in found.get(key, []):
            print("  %s slot 0x%x lies in %s, nearest object: %s" % (key, a, secn, owner(a)))
    print("\nall memcpy call sites by function:")
    from collections import Counter
    print(Counter(s[1] for s in sites).most_common())
    print("\nident_info_funcs entries (id -> builder):")
    for a, ident, fn in entries:
        print("  0x%x id=0x%02x fn=0x%x %s" % (a, ident, fn, func_at(fn)[0]))


if __name__ == "__main__":
    main(sys.argv[1])
