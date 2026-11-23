# Where it works

Reports from cars, taken from public issues in this repository. Every car here runs
`MHI2_ER_AUG22_K3346`, MU 1438: the installer refuses any other firmware without changing anything.

Is your car missing? Open an issue and tell me the model, the year, the cluster layout you use,
whether you have a HUD, and what works. I'll add it here.

| Car | Cluster / HUD | Mod version | What was reported | Report |
|---|---|---|---|---|
| Audi A4 B9 (my car) | classic layout, no HUD | 1.0.0 to 2.0.0 | Everything in the README is tested here, with the iPhone on the cable. | |
| Audi A4 B9, 2016 | classic layout | 1.0.0, 1.0.1 | 1.0.0: the cluster map went blank during route guidance; NavIgnore is required. 1.0.1: route guidance, map zoom, touchpad arrows, phone and music work; the map takes 10–12 s to appear on the cluster after connecting. | [#5](../../issues/5), [#11](../../issues/11) |
| Audi A4 B9, 2016 | not stated | 1.0.0 | Map and route guidance work; after unplugging and plugging the cable back the cluster map stayed black until an MMI restart. Open. | [#7](../../issues/7) |
| Audi A4 B9, 2016 | cluster hardware H15, version 0379; Sport layout | 1.0.1 | Sport layout: only part of the map is visible, and the factory map shows for a few seconds first. Confirmed by a second A4 B9. With the classic variant coded back, another user no longer saw the factory map first. | [#8](../../issues/8), [#9](../../issues/9) |
| Audi Q7 4M | cluster H13, version 0378; HUD | 1.0.0, 1.0.1 | Installed without errors. HUD shows the arrow and the distance, but with lag. Touchpad in factory mode works with an Android-over-CarPlay box. | [#6](../../issues/6) |
| Audi Q7 e-tron 3.0 TDI, 2016 | HUD | not stated | Map and arrows work. HUD arrows work, but the turn stays on the HUD the whole route even with "display only before maneuver" set. | [#15](../../issues/15), [#1](../../issues/1) |
| not stated | HUD | 1.0.1, 2.0.0 | HUD arrows work. 1.0.1: lane arrows under the maneuver kept an earlier maneuver. 2.0.0: fixed, confirmed by the reporter. | [#1](../../issues/1), [#16](../../issues/16) |
| Audi A4 B9, 2016 | analogue cluster, no Virtual Cockpit; HUD | not stated | Installed without errors. The HUD shows directions from Apple Maps and Google Maps. I can't test this setup, so it is not supported. | [#22](../../issues/22) |

## Not supported yet

- **S4 Sport cluster layout**, also when it is only coded and not selected: the map sits partly
  behind the rev counter and the factory map shows for a moment after VIEW ([#8](../../issues/8)).
- **Wireless CarPlay adapters:** the CarPlay map often does not start on the cluster and the
  factory map stays ([#10](../../issues/10)). Use the cable.
- **Any other firmware** than `MHI2_ER_AUG22_K3346`, MU 1438.
