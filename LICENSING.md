# Licensing

This repository combines material with different provenance. No single license covers all of it.

| Part | Origin | Terms |
|---|---|---|
| `port_k3346/altscreen/**` (files present in the upstream tree) | [edzenis/mib2high-carplay-altscreen-rgi](https://github.com/edzenis/mib2high-carplay-altscreen-rgi) @ `bfea868` (see `port_k3346/UPSTREAM.txt`) | as stated in `port_k3346/LICENSE` and `port_k3346/NOTICE.md` (mixed; renderer GPL-3.0) |
| Code adapted from LuKa | [luka-dev/mib2q-carplay-rgi](https://github.com/luka-dev/mib2q-carplay-rgi) | GPL-3.0-or-later (per that project's NOTICE); original copyright lines retained |
| Changes and new files written for this K3346 port (K3346 offsets, Stage 4 RGI-in-`mm-ipod` launcher and installer, Stage 5 Java logic, tools, docs) | this repository | GPL-3.0-or-later, see [LICENSE](LICENSE) |
| `port_k3346/altscreen/java/overrides/**` classes with Audi package names | rewritten from decompiled Audi MHI2 K3346 HMI classes, with minimal changes, for interoperability | **not licensed by this repository**; the original code belongs to its copyright holders. Provided as-is for interoperability research, like comparable public projects. Remove them if this is a concern for you |

Nothing here is legal advice. No warranty of any kind.
