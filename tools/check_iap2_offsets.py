#!/usr/bin/env python3
"""Check edzenis K2161 hard-coded offsets against a stock ipod-drvr-iap2.so.

Read-only. Offsets are ELF virtual addresses (DYN, base 0) as used by
edzenis native/{observer,ident_hook,message_table,send_hook}.c.

usage: check_iap2_offsets.py ipod-drvr-iap2.so
"""
import struct
import sys

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

EXPECT_SYMS = {  # symbol -> K2161 VA used by observer.c
    "ipod_module": 0x2614C,
    "packet_addparam_blob": 0x1D0D8,
}
# K2161 function VAs used by send_hook.c / ident_hook.c (should be function starts)
FUNCS = {
    "IAP2_MSG (iap2 msg handler)": 0x0E484,
    "IAP2_CLOSE": 0x0F760,
    "LINK_SEND_MESSAGE": 0x10530,
    "ADDPARAM_NODATA": 0x1CF50,
    "ADDPARAM_NUM": 0x1D210,
    "SENDLIST_PUSH": 0x1DC14,
    "SENDLIST_POP": 0x1DFAC,
    "GPS_BUILDER": 0x0A11C,
    "ADDPARAM_BLOB": 0x1D0D8,
}
# K2161 data slots
SLOTS = {
    "IDENT_ID (expect 0x0016)": 0xA7D80,
    "IDENT_BUILDER slot (expect ->GPS_BUILDER 0xa11c)": 0xA7D84,
    "IAP2_MSG_SLOT (expect ->0xe484)": 0xA7DAC,
    "IAP2_CLOSE_SLOT (expect ->0xf760)": 0xA7DC0,
}
MSG_TABLE = 0xAB050
STRINGS = {
    "GROUP50_NAME": 0x237DC,
    "GROUP68_NAME": 0x237E8,
    "NP_START_NAME": 0x23B80,
    "NP_UPDATE_NAME": 0x23B98,
}
RX_CALLSITE_RET = 0x11428  # memcpy return address inside driver


def main(path):
    f = open(path, "rb")
    elf = ELFFile(f)
    data = open(path, "rb").read()

    def va2off(va):
        for seg in elf.iter_segments():
            if seg["p_type"] == "PT_LOAD" and seg["p_vaddr"] <= va < seg["p_vaddr"] + seg["p_filesz"]:
                return va - seg["p_vaddr"] + seg["p_offset"]
        return None

    def u32(va):
        o = va2off(va)
        return None if o is None else struct.unpack_from("<I", data, o)[0]

    def cstr(va):
        o = va2off(va)
        if o is None:
            return None
        e = data.index(b"\0", o)
        return data[o:e].decode("latin-1")

    # relocations: map of VA -> (type, symbol name or None, addend)
    relocs = {}
    for sec in elf.iter_sections():
        if isinstance(sec, RelocationSection):
            symtab = elf.get_section(sec["sh_link"])
            for r in sec.iter_relocations():
                name = None
                if r["r_info_sym"]:
                    name = symtab.get_symbol(r["r_info_sym"]).name
                relocs[r["r_offset"]] = (r["r_info_type"], name)

    syms = {}
    for secname in (".dynsym", ".symtab"):
        s = elf.get_section_by_name(secname)
        if s:
            for sym in s.iter_symbols():
                if sym.name:
                    syms.setdefault(sym.name, sym["st_value"])
    by_addr = {}
    for n, v in syms.items():
        by_addr.setdefault(v & ~1, []).append(n)

    print("file:", path)
    print("symbols: %d (dynsym+symtab)" % len(syms))
    print("\n[exported symbols used by observer.c]")
    for n, exp in EXPECT_SYMS.items():
        got = syms.get(n)
        print("  %-22s K2161=0x%05x  here=%s  %s" % (n, exp, hex(got) if got is not None else "MISSING",
                                                       "OK" if got == exp else "DIFF"))
    print("\n[function VAs from send_hook.c/ident_hook.c]")
    for n, va in FUNCS.items():
        names = by_addr.get(va, [])
        word = u32(va)
        print("  %-28s 0x%05x  sym=%s  first_insn=%s" % (n, va, ",".join(names) or "-",
                                                       ("%08x" % word) if word is not None else "n/a"))
    print("\n[data slots]")
    for n, va in SLOTS.items():
        v = u32(va)
        rel = relocs.get(va)
        print("  %-48s 0x%05x  value=%s  reloc=%s" % (n, va, hex(v) if v is not None else "n/a", rel))
    print("\n[strings referenced by message_table.c]")
    for n, va in STRINGS.items():
        print("  %-16s 0x%05x  %r" % (n, va, cstr(va)))
    print("\n[control message table 0x%x: groups 0x50,0x52,0x68 = {name*, msgs*, count}]" % MSG_TABLE)
    for g in (0x50, 0x52, 0x68):
        base = MSG_TABLE + g * 12
        name_p, msgs_p, cnt = u32(base), u32(base + 4), u32(base + 8)
        print("  group 0x%02x @0x%x name=%s(%r) msgs=%s count=%s" % (
            g, base, hex(name_p) if name_p is not None else None,
            cstr(name_p) if name_p else None, hex(msgs_p) if msgs_p is not None else None, cnt))
    print("\n[memcpy RX callsite] return VA 0x%x, preceding insn = %08x" % (RX_CALLSITE_RET, u32(RX_CALLSITE_RET - 4) or 0))
    near = sorted((a for a in by_addr if a <= RX_CALLSITE_RET), reverse=True)[:1]
    if near:
        print("  inside function:", by_addr[near[0]], "@", hex(near[0]))
    # ident: show what IDENT slot points to
    print()


if __name__ == "__main__":
    main(sys.argv[1])
