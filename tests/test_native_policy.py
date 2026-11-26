"""Native update boundaries must reject before networking or privileged work."""
import importlib.util
import subprocess
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("native_system", ROOT / "backend/library/system.py")
system = importlib.util.module_from_spec(spec)
spec.loader.exec_module(system)


class NativePolicyTest(unittest.TestCase):
    def test_memory_reports_available_column_instead_of_free(self):
        output = "               total used free shared buff/cache available\nMem: 1.0Gi 200Mi 20Mi 10Mi 800Mi 700Mi\n"
        with patch.object(system, "_run", return_value=subprocess.CompletedProcess([], 0, output, "")):
            self.assertEqual(system._memory_usage()["available"], "700Mi")

    def test_native_api_never_checks_or_installs_a_release(self):
        with patch.object(system, "_remote_release", side_effect=AssertionError("network")), \
             patch.object(system, "_run", side_effect=AssertionError("subprocess")):
            for action in (system.check_update, system.get_update_status, system.apply_update):
                status = action(ROOT)
                self.assertFalse(status["supported"])
                self.assertFalse(status["applying"])
                self.assertEqual(status["repository"], "BASF2HD/PiTunes-Native")
            self.assertFalse(system.apply_update(ROOT)["ok"])

    def test_update_scripts_refuse_direct_execution(self):
        for script in ("pitunes-update.sh", "pitunes-system-update.sh"):
            result = subprocess.run(["bash", str(ROOT / "scripts" / script)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertIn("Native OTA is disabled", result.stderr)


if __name__ == "__main__":
    unittest.main()
