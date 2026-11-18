"""Host test of scripts/analyze_hook_so_levels.py with a synthetic TDM capture (no emulator)."""
import json, subprocess, sys, tempfile, unittest
from pathlib import Path
import numpy as np
ROOT = Path(__file__).resolve().parents[1]
S16_BYTES = [(0, 1), (5, 6), (10, 11), (15, 8), (12, 13), (17, 18)]

def frames(values, n):
    raw = bytearray(32 * n)
    for i in range(n):
        for (lo, hi), v in zip(S16_BYTES, values):
            raw[32 * i + lo] = v & 255; raw[32 * i + hi] = (v >> 8) & 255
    return bytes(raw)

class AnalyzeLevels(unittest.TestCase):
    def test_groups_by_marker_and_measures_rms(self):
        with tempfile.TemporaryDirectory() as d:
            run = Path(d); raw = b""; idx = []
            for marker, vals in ((200, [1000, 1000, 0, 0, 0, 0]), (204, [1000, 1000, 0, 0, 100, 100]), (0, [7, 7, 7, 7, 7, 7])):
                blk = frames(vals, 64); idx.append("0 %d %d %d" % (len(raw), len(blk), marker)); raw += blk
            (run / "native-tdm0.raw").write_bytes(raw); (run / "native-tdm0.index").write_text("\n".join(idx) + "\n")
            subprocess.run([sys.executable, str(ROOT / "scripts/analyze_hook_so_levels.py"), str(run)], check=True, capture_output=True)
            res = json.loads((run / "level-rms.json").read_text())
            self.assertEqual(sorted(res), ["0", "4"])          # marker 0 (idle) is ignored
            self.assertEqual(res["0"]["rms"]["RL"], 0.0); self.assertEqual(res["0"]["rms"]["FL"], 1000.0)
            self.assertEqual(res["4"]["rms"]["RL"], 100.0); self.assertEqual(res["4"]["rms"]["RR"], 100.0)
if __name__ == "__main__": unittest.main()
