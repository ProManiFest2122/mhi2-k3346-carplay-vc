# Licensing

This repository combines material with different provenance. No single license covers all of it.

| Part | Origin | Terms |
|---|---|---|
| `port_k3346/altscreen/**` (files present in the upstream tree) | [edzenis/mib2high-carplay-altscreen-rgi](https://github.com/edzenis/mib2high-carplay-altscreen-rgi) @ `bfea868` (see `port_k3346/UPSTREAM.txt`) | as stated in `port_k3346/LICENSE` and `port_k3346/NOTICE.md` (mixed; renderer GPL-3.0) |
| Code adapted from LuKa | [luka-dev/mib2q-carplay-rgi](https://github.com/luka-dev/mib2q-carplay-rgi) | GPL-3.0-or-later (per that project's NOTICE); original copyright lines retained |
| Facts taken from LuKa's reverse-engineering notes (not code): `docs/reference/MHI2_MU1439_PORT.md` — the OpenKODE window recipe, caption semantics, premultiplied-alpha compositing and KVS_FPK panel sizes | same project | documentation under that project's terms; cited, not copied. Our implementation is written against our own firmware and our own reading of its binaries (see `docs/19`) |
| `port_k3346/altscreen/coverart/stb_image.h` | [nothings/stb](https://github.com/nothings/stb) v2.30, unmodified | public domain (Unlicense) or MIT, as stated at the end of the file |
| Changes and new files written for this K3346 port (K3346 offsets, Stage 4 RGI-in-`mm-ipod` launcher and installer, Stage 5 Java logic, tools, docs) | this repository | GPL-3.0-or-later, see [LICENSE](LICENSE) |
| `port_k3346/altscreen/java/overrides/**` classes with Audi package names | rewritten from decompiled Audi MHI2 K3346 HMI classes, with minimal changes, for interoperability | **not licensed by this repository**; the original code belongs to its copyright holders. Provided as-is for interoperability research, like comparable public projects. Remove them if this is a concern for you |
| *(nothing in this repository)* | [chefranov/mhi2-au37x-carplay](https://github.com/chefranov/mhi2-au37x-carplay) | That project is published under the author's own terms, which are **not** an open-source licence: they forbid sale, distribution of modified versions, and distribution of anything derived from its binaries by reverse engineering. **No file, binary, jar, shader or script from it is present here.** It helped as a pointer only — see Credits in the README for exactly what. |

Nothing here is legal advice. No warranty of any kind.
