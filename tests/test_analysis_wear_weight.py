"""Optional Python analysis tests; run with unittest discover.

Fixture values are synthetic and deliberately do not represent SSD results.
"""

from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import pandas as pd

REPO = Path(__file__).resolve().parents[1]
ANALYZER = REPO / "scripts" / "analyze_wear_weight.py"

spec = importlib.util.spec_from_file_location("wear_weight_analysis", ANALYZER)
assert spec is not None and spec.loader is not None
analysis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analysis)


def synthetic_benchmark() -> pd.DataFrame:
    rows = []
    for logical_pages in (400, 440, 464, 480):
        for workload in ("Random", "HotCold"):
            for seed in (42, 43, 44, 45, 46):
                base_waf = (2.0 + (logical_pages - 400) / 80 +
                            0.01 * (seed - 42) + (0.4 if workload == "Random" else 0))
                for policy, lam in [
                    ("Greedy", 0.0), ("FIFO", 0.0),
                    *(("WearAwareGreedy", x) for x in (0, 0.01, 0.02, 0.05, 0.1, 0.2)),
                ]:
                    if policy == "FIFO":
                        waf = base_waf + 0.2
                        cv = 0.025
                        max_erase = 78
                    elif policy == "WearAwareGreedy" and lam > 0:
                        waf = base_waf + 0.015 + lam * 0.05
                        cv = 0.07 - lam * 0.1
                        max_erase = 80
                    else:
                        waf = base_waf
                        cv = 0.15
                        max_erase = 95
                    rows.append({
                        "logical_pages": logical_pages,
                        "workload": workload,
                        "seed": seed,
                        "policy": policy,
                        "lambda": lam,
                        "attempted": 10000,
                        "successful": 10000,
                        "success_rate": 1.0,
                        "waf": waf,
                        "erase_cv": cv,
                        "max_erases": max_erase,
                        "gc_cycles": 500,
                        "total_erases": 500,
                    })
    return pd.DataFrame(rows)


class WearWeightAnalysisTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.df = synthetic_benchmark()

    def test_shapes_and_baseline_matching(self):
        greedy, wear = analysis.validate_input(self.df)
        summary = analysis.make_summary(wear)
        paired = analysis.make_paired(wear, greedy)
        ci = analysis.make_ci(paired)
        pareto = analysis.make_pareto(self.df)
        self.assertEqual((len(self.df), len(summary), len(paired), len(ci)),
                         (320, 48, 240, 144))
        self.assertTrue(summary["std_waf"].notna().all())
        self.assertFalse(pareto.empty)
        wear_zero = paired.loc[paired["lambda"] == 0]
        self.assertTrue((wear_zero["delta_waf"] == 0).all())
        self.assertTrue((wear_zero["delta_cv"] == 0).all())

    def test_duplicate_combination_is_rejected(self):
        broken = self.df.copy()
        broken.iloc[-1] = broken.iloc[0]
        with self.assertRaises(AssertionError):
            analysis.validate_input(broken)

    def test_cli_outputs_expected_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            results = tmp_path / "results"
            results.mkdir()
            self.df.to_csv(results / "wear_weight_sensitivity.csv", index=False)
            proc = subprocess.run(
                [sys.executable, str(ANALYZER)],
                cwd=tmp_path, text=True, capture_output=True, check=False,
            )
            self.assertEqual(proc.returncode, 0, msg=proc.stdout + proc.stderr)
            for filename in (
                "wear_weight_summary_by_utilization.csv",
                "wear_weight_paired_deltas.csv",
                "wear_weight_paired_ci95.csv",
                "wear_weight_pareto_by_utilization.csv",
            ):
                self.assertTrue((results / filename).is_file(), filename)
            images = list((results / "wear_weight_plots").glob("*.png"))
            self.assertEqual(len(images), 10)


if __name__ == "__main__":
    unittest.main()
