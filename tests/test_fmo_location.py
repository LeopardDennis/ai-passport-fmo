"""Real location/cache/JSON logic, built against the pinned IDF cJSON."""
import os
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
idf = os.environ.get("IDF_PATH")
if not idf:
    if os.environ.get("FMO_REQUIRE_NETWORK_TEST"):
        raise RuntimeError("Activate ESP-IDF to test the location JSON parser")
    print("FMO location: SKIP (activate ESP-IDF; required by firmware gate)")
    raise SystemExit(0)
cjson = Path(idf) / "components/json/cJSON"
with tempfile.TemporaryDirectory(prefix="fmo-location-test-") as tmp:
    binary = Path(tmp) / "location"
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(ROOT / "main"), "-I" + str(cjson),
                    str(ROOT / "tests/test_fmo_location.c"), str(cjson / "cJSON.c"),
                    *[str(ROOT / "main" / f) for f in ("fmo_location.c", "fmo_location_json.c", "fmo_monitor_state.c", "fmo_text.c")],
                    "-lm", "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
