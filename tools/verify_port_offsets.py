#!/usr/bin/env python3
"""Offline simulation of the fail-closed runtime checks in the ported native
sources (observer.c, ident_hook.c, message_table.c, send_hook.c) against a
stock ipod-drvr-iap2.so. Constants are parsed from the C sources, so this
verifies exactly what will be compiled. Read-only.

usage: verify_port_offsets.py NATIVE_DIR ipod-drvr-iap2.so
"""
import re
import struct
import sys

from elftools.elf.elffile import ELFFile


def consts(path):
    src = open(path, encoding="utf-8", errors="replace").read()
    return {k: int(v, 16) for k, v in re.findall(r"\b([A-Z0-9_]+)\s*=\s*(0x[0-9a-fA-F]+)", src)}


def main(native, drv):
    raw = open(drv, "rb").read()
    elf = ELFFile(open(drv, "rb"))

    def off(va):
        for s in elf.iter_segments():
            if s["p_type"] == "PT_LOAD" and s["p_vaddr"] <= va < s["p_vaddr"] + s["p_filesz"]:
                return va - s["p_vaddr"] + s["p_offset"]
        raise ValueError("VA 0x%x not in file" % va)

    u32 = lambda va: struct.unpack_from("<I", raw, off(va))[0]
    syms = {s.name: s["st_value"] for s in elf.get_section_by_name(".dynsym").iter_symbols() if s.name}
    ok = True

    def check(name, cond, detail=""):
        nonlocal ok
        ok &= bool(cond)
        print("%-4s %s %s" % ("PASS" if cond else "FAIL", name, detail))

    obs = open(native + "/observer.c", encoding="utf-8", errors="replace").read()
    a_mod = int(re.search(r"ipod_module - \(uintptr_t\)(0x[0-9a-fA-F]+)", obs).group(1), 16)
    a_blob = int(re.search(r"blob_symbol - \(uintptr_t\)(0x[0-9a-fA-F]+)", obs).group(1), 16)
    rx = int(re.search(r"driver_base \+ \(uintptr_t\)(0x[0-9a-fA-F]+)", obs).group(1), 16)
    b1, b2 = syms["ipod_module"] - a_mod, syms["packet_addparam_blob"] - a_blob
    check("observer: dual-anchor base crosscheck", b1 == b2 == 0, "base=%d/%d" % (b1, b2))
    w = u32(rx - 4)
    tgt = (rx - 4) + 8 + ((((w & 0xFFFFFF) ^ 0x800000) - 0x800000) << 2)
    rp = elf.get_section_by_name(".rel.plt")
    st = elf.get_section(rp["sh_link"])
    names = [st.get_symbol(r["r_info_sym"]).name for r in rp.iter_relocations()]
    memcpy_plt = elf.get_section_by_name(".plt")["sh_addr"] + 20 + 12 * names.index("memcpy")
    check("observer: RX callsite is 'bl memcpy'", (w & 0x0F000000) == 0x0B000000 and tgt == memcpy_plt,
          "ret=0x%x" % rx)

    ih = consts(native + "/ident_hook.c")
    check("ident: id == 0x0016", u32(ih["K2161_IDENT_ID_OFFSET"]) == 0x16)
    check("ident: builder slot -> GPS builder", u32(ih["K2161_IDENT_BUILDER_OFFSET"]) == ih["K2161_GPS_BUILDER_OFFSET"])
    check("ident: addparam_blob == export", ih["K2161_ADDPARAM_BLOB_OFFSET"] == syms["packet_addparam_blob"])

    mt = consts(native + "/message_table.c")
    t = mt["K2161_CTRL_MSG_TABLE_OFFSET"]
    g = lambda grp: (u32(t + 12 * grp), u32(t + 12 * grp + 4), u32(t + 12 * grp + 8))
    g50, g52, g68 = g(0x50), g(0x52), g(0x68)
    check("msgtable: group50", g50 == (mt["K2161_GROUP50_NAME_OFFSET"], mt["K2161_GROUP50_MESSAGES_OFFSET"], 4))
    check("msgtable: group68", g68 == (mt["K2161_GROUP68_NAME_OFFSET"], mt["K2161_GROUP68_MESSAGES_OFFSET"], 4))
    m = mt["K2161_GROUP50_MESSAGES_OFFSET"]
    check("msgtable: NP start", (u32(m), u32(m + 4), u32(m + 8)) ==
          (mt["K2161_NP_START_NAME_OFFSET"], 1, mt["K2161_NP_START_PARAMS_OFFSET"]))
    check("msgtable: NP update", (u32(m + 12), u32(m + 16), u32(m + 20)) ==
          (mt["K2161_NP_UPDATE_NAME_OFFSET"], 2, mt["K2161_NP_UPDATE_PARAMS_OFFSET"]))
    check("msgtable: group52 stock empty", g52 == (0, 0, 0))

    sh = consts(native + "/send_hook.c")
    check("send: msg slot -> iap2_msg", u32(sh["IAP2_MSG_SLOT_OFFSET"]) == sh["IAP2_MSG_OFFSET"] == syms["iap2_msg"])
    check("send: close slot -> iap2_ocb_close",
          u32(sh["IAP2_CLOSE_SLOT_OFFSET"]) == sh["IAP2_CLOSE_OFFSET"] == syms["iap2_ocb_close"])
    for k, s in (("ADDPARAM_NODATA_OFFSET", "packet_addparam_nodata"), ("ADDPARAM_NUM_OFFSET", "packet_addparam_num"),
                 ("SENDLIST_POP_OFFSET", "packet_sendlist_pop"), ("SENDLIST_PUSH_OFFSET", "packet_sendlist_push"),
                 ("LINK_SEND_MESSAGE_OFFSET", "link_send_message")):
        check("send: %s == %s" % (k, s), sh[k] == syms[s])
    print("RESULT:", "ALL PASS" if ok else "FAILURES")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))
