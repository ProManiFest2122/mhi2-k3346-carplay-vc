# AltScreen v1.0

**Apple CarPlay navigation inside the digital instrument cluster.** This is the first public AltScreen release based on the car-tested **v8S** software, supplied as both the **full editable source archive** and the **compiled SD-card installer**.

## What it does

The CarPlay map is shown beside the speedometer and tachometer. AltScreen follows the cluster's small/big gauge layouts and requests the appropriate CarPlay instruction card placement when VIEW is changed. A separate route-guidance (RGI) interface supplies turn symbols and maneuver distances to the factory HUD on the tested vehicle.

## What's included

- Working auxiliary H.264 map video at 1440×542 on the reference vehicle.
- Small-gauge **left** and big-gauge **top** instruction card layouts; serialized and acknowledged control messages for VIEW switching and reconnects.
- Factory HUD turn-by-turn data through the existing BAP route-guidance interface.
- SD-card installer with backup, diagnostics collection and exact rollback.
- **Two assets:** `AltScreen-v1.0-source.zip` (editable C/Java, configs, scripts and tests), `AltScreen-v1.0-install.zip` (unchanged, compiled car-tested v8S installer).

## Compatibility

**Car-tested:** Audi Q7 4M, Harman MIB2 High K2161/MU1421, compatible digital cockpit and HUD. **Other Audi, VW, Škoda firmware/hardware and wireless adapters are not certified by this release.**

## Known issue

Factory Audi navigation/compass/tile graphics can appear with CarPlay navigation in the VC. HUD-versus-VC guidance separation is under investigation; no untested suppression changes have been inserted into v1.0.

## Installation and recovery

Unzip the installer ZIP to the SD-card root, run M.I.B. `mod/custom.sh`, then reboot MMI. Keep the backups. For rollback create `mod/mib2high-carplay-rgi-v8s-simple/ROLLBACK` and rerun custom script, then reboot. Read the asset `README.txt` before attempting installation; use only on the validated platform with recovery access.

The original v8S installer README was authored before the final car test and still describes its *then-current* offline validation status. **The later v8S car test confirmed the left instruction card layout; that archived README is intentionally not rewritten because the binary package is unchanged.** Dynamic layout behavior can vary by app and session.

See `altscreen/README.md` and `altscreen/BUILDING.md` in the repository for features, architecture, caveats and development instructions.


## Public source distribution clarification (post-release)

The v1.0 compiled installer is **unchanged**. Source-publication updates provide the exact installer AWK scripts and a pinned upstream copy of the GPL-3.0 renderer source, plus updated instructions and integrity tests. Exact historical v8S renderer **binary/source parity is still unproven**; these additions must not be described as a full bit-for-bit reproducible build. The release ZIP source asset may be refreshed with documentation/source complements without modifying the tested installer.
