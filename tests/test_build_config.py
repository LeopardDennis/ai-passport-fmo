import importlib.util
import tempfile
import os
import shutil
import subprocess
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("config_check", ROOT / "tools/check_fmo_config.py")
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


class BuildConfigTests(unittest.TestCase):
    def test_packaging_config_isolation(self):
        # Stop at the first build call: verify its config input without IDF.
        for mode in ("--configured", "--firmware", "--setup"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / "tools").mkdir()
                (root / "bin").mkdir()
                for name in ("validate.sh", "check_fmo_config.py"):
                    shutil.copy(ROOT / "tools" / name, root / "tools" / name)
                configuration = 'CONFIG_FMO_WIFI_SSID="private-build-test"\nCONFIG_FMO_HOST="192.0.2.9"\n'
                (root / "sdkconfig").write_text(configuration)
                probe = root / "bin" / "idf.py"
                probe.write_text("#!" + os.sys.executable + "\n" +
                    "import json, os, sys\nfrom pathlib import Path\n" +
                    "config = Path(next(x.split('=',1)[1] for x in sys.argv if x.startswith('SDKCONFIG=')))\n" +
                    "Path(os.environ['FMO_PROBE']).write_text(json.dumps({'defaults': os.environ['SDKCONFIG_DEFAULTS'], " +
                    "'config': config.read_text() if config.exists() else None}))\nsys.exit(77)\n")
                probe.chmod(0o755)
                env = dict(os.environ, PATH=str(root / "bin") + os.pathsep + os.environ["PATH"],
                           FMO_PROBE=str(root / "probe.json"))
                result = subprocess.run(["bash", str(root / "tools/validate.sh"), mode],
                                        env=env, capture_output=True, text=True)
                self.assertEqual(result.returncode, 77, result.stderr)
                recorded = json.loads((root / "probe.json").read_text())
                self.assertEqual((root / "sdkconfig").read_text(), configuration)
                if mode == "--configured":
                    self.assertEqual(recorded["config"], configuration)
                    self.assertNotIn("sdkconfig.network", recorded["defaults"])
                elif mode == "--firmware":
                    self.assertIsNone(recorded["config"])
                    self.assertIn("sdkconfig.network", recorded["defaults"])
                else:
                    self.assertIsNone(recorded["config"])
                    self.assertNotIn("sdkconfig.network", recorded["defaults"])

    def check_config(self, ssid, host):
        import json
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sdkconfig"
            path.write_text("CONFIG_FMO_WIFI_SSID=" + json.dumps(ssid) +
                            "\nCONFIG_FMO_HOST=" + json.dumps(host) + "\n")
            CHECK.validate(path)

    def test_valid(self):
        self.check_config("x" * 32, "192.0.2.1")

    def test_empty_and_invalid(self):
        for ssid, host in [("", "192.0.2.1"), ("x" * 33, "192.0.2.1"),
                           ("demo", ""), ("demo", "ws://192.0.2.1")]:
            with self.assertRaises(ValueError):
                self.check_config(ssid, host)


if __name__ == "__main__":
    unittest.main()
