"""Summarize saved Aura PIE results without replaying or modifying the editor."""
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def sequence(name):
    result = json.loads((ROOT / name).read_text(encoding="utf-8"))
    for item in result:
        for content in item["result"].get("content", []):
            if content.get("type") == "text":
                value = json.loads(content["text"])
                if isinstance(value, dict) and "results" in value:
                    return value
    raise RuntimeError("No sequence result in " + name)


up = sequence("ride_up_result.json")
controls = sequence("controls_test_result.json")
assert all(step["ok"] for step in up["results"] + controls["results"])
up_data = {step["index"]:step["result"].get("data", {}) for step in up["results"]}
data = {step["index"]:step["result"].get("data", {}) for step in controls["results"]}
rows = up_data[10]["rows"]
assert all(b[1] >= a[1] - 0.01 for a,b in zip(rows, rows[1:])), "Uphill backtrack"
assert max(abs(row[2] - 800) for row in rows) < 0.01, "Unexpected lateral drift"
max_speed = max(math.dist(a[1:], b[1:]) / (b[0]-a[0]) for a,b in zip(rows,rows[1:]))
assert max_speed < 100, "Unexpected uphill position jump"
assert abs(up_data[7]["location"]["z"]-up_data[2]["location"]["z"]-360) < 0.1
assert up_data[7]["location"] == up_data[9]["location"], "Unstable upper exit"
assert data[3]["location"]["x"] > 1983.54, "Upper exit not reached by walking"
assert abs(data[8]["location"]["x"]-1145) < 0.1, "Downhill exit not reached"
assert data[6]["velocity"]["speed"] == data[8]["velocity"]["speed"] == 0
assert data[11]["location"]["x"] < 1100, "Lower exit not reached by walking"
assert data[16]["location"] == data[18]["location"], "Stopped belt still carrying"
assert data[21]["location"]["x"] > data[18]["location"]["x"]+20, "Restart did not carry"
assert abs(data[24]["location"]["x"]-data[26]["location"]["x"]) < 0.01, "Belt carried airborne player"
assert data[24]["velocity"]["z"] > 0 and data[26]["location"]["z"] > data[24]["location"]["z"]

summary = {
    "uphill_rise_cm": round(up_data[7]["location"]["z"]-up_data[2]["location"]["z"], 3),
    "uphill_samples":len(rows), "uphill_max_sampled_speed_cm_s":round(max_speed,3),
    "checks_passed":["uphill carry through both seams","stable upper exit", "downhill carry without input",
                     "walking off upper and lower exits","stop and restart", "jump excludes belt carry"],
    "geometry_at_test":{"up_origin":[1100,800,6],"down_origin":[1100,1050,6]},
    "final_placement_translation_cm":[0,-250,0],
    "screenshots":controls.get("screenshot_paths",[]),
    "note":"Primary ride/control checks preceded a translation to clear an existing obstacle. Native persistent-build/reload check follows separately.",
}
output = ROOT.parents[1] / "Saved" / "EscalatorVerify" / "verification_summary.json"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(summary, indent=2), encoding="utf-8")
print(json.dumps(summary, separators=(",", ":")))
