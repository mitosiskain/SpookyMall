"""Summarize saved Aura verification results without accessing Unreal.

Run again as store/gallery result files arrive. Nested step flags and physical
measurements are authoritative; an outer success flag alone never passes a test.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re


REQUIRED = {
    "up_ride": "clean_up_verify_result.json",
    "down_ride": "clean_down_landing_verify_result.json",
    "store_entry": "stores_verify_result.json",
    "gallery_loop": "gallery_final_verify_result.json",
    "fountain_visual": "fountain_verify_result.json",
    "gallery_guards": "gallery_guards_verify_result.json",
}
EXCLUDED = {
    "clean_down_verify_result.json": "Invalid setup: ground snapping placed the player on the roof; superseded by clean_down_landing_verify_result.json.",
    "rides_verify_result.json": "Earlier verification before the background construction run was stopped; excluded from clean acceptance evidence.",
    "gallery_verify_result.json": "Initial continuous gallery circuit fell from Z542.15 to Z92.15 during its first leg. Retained for diagnosis, excluded from acceptance; superseded by the corrected retest.",
    "gallery_corrected_verify_result.json": "Section-collision refresh passed a short probe but the first full retest still fell to ground. Missing transition slabs were repaired afterward; retained as diagnosis, excluded from final acceptance.",
}


def vector(value):
    if isinstance(value, dict) and all(k in value for k in ("x", "y", "z")):
        return [value[k] for k in ("x", "y", "z")]
    if isinstance(value, list) and len(value) >= 3:
        return value[:3]
    return None


def collect_sequences(value, sequences, errors):
    if isinstance(value, list):
        for item in value:
            collect_sequences(item, sequences, errors)
    elif isinstance(value, dict):
        if value.get("isError") is True:
            errors.append("MCP result isError=true")
        if isinstance(value.get("results"), list):
            sequences.append(value)
            return
        for key, item in value.items():
            if key == "text" and isinstance(item, str):
                try:
                    decoded = json.loads(item)
                except json.JSONDecodeError:
                    continue
                collect_sequences(decoded, sequences, errors)
            elif isinstance(item, (dict, list)):
                collect_sequences(item, sequences, errors)


def collect_request_actions(value):
    if isinstance(value, list):
        return [action for item in value for action in collect_request_actions(item)]
    if not isinstance(value, dict):
        return []
    if value.get("tool") == "run_verification_sequence":
        return value.get("arguments", {}).get("actions", [])
    return [action for item in value.values() if isinstance(item, (dict, list)) for action in collect_request_actions(item)]


def summarize_test(path, request_path=None):
    if not path.exists():
        return {"file": str(path), "status": "pending", "reason": "Result file not present.", "execution_pass": False}
    raw = path.read_bytes()
    result = {"file": str(path), "sha256": hashlib.sha256(raw).hexdigest(), "bytes": len(raw)}
    errors, sequences = [], []
    try:
        collect_sequences(json.loads(raw.decode("utf-8-sig")), sequences, errors)
    except (UnicodeError, json.JSONDecodeError) as exc:
        return {**result, "status": "invalid", "execution_pass": False, "errors": [str(exc)]}
    if not sequences:
        errors.append("No verification sequence with nested results was found.")
    request_path = request_path or path.with_name(path.name.replace("_result.json", "_request.json"))
    actions = []
    if request_path.exists():
        try:
            request = json.loads(request_path.read_text(encoding="utf-8-sig"))
            actions = collect_request_actions(request)
        except (UnicodeError, json.JSONDecodeError) as exc:
            errors.append(f"Invalid request file: {exc}")
    steps, states, screenshots, modes, commands, windows, properties = [], [], [], [], [], [], []
    for sequence_number, sequence in enumerate(sequences):
        if sequence.get("success") is not True:
            errors.append(f"Sequence {sequence_number}: outer success is not true.")
        if not sequence["results"]:
            errors.append(f"Sequence {sequence_number}: no steps.")
        if sequence.get("recording", {}).get("act_window"):
            windows.append(sequence["recording"]["act_window"])
        for position, step in enumerate(sequence["results"]):
            nested = step.get("result", {})
            if not isinstance(nested, dict):
                nested = {}
            check = {
                "sequence": sequence_number, "index": step.get("index", position),
                "type": step.get("type"), "ok": step.get("ok") is True,
                "result_success": nested.get("success") is True,
                "pie_time_seconds": nested.get("pie_time_seconds", nested.get("game_time_seconds")),
            }
            check["pass"] = check["ok"] and check["result_success"]
            if check["index"] != position:
                errors.append(f"Sequence {sequence_number}: step index {check['index']} does not match position {position}.")
            if not check["pass"]:
                check["error"] = nested.get("error", nested.get("message", "Missing or false nested success/ok flag."))
                errors.append(f"Sequence {sequence_number}, step {check['index']}: {check['error']}")
            steps.append(check)
            data = nested.get("data", {})
            if not isinstance(data, dict):
                data = {}
            state = data if step.get("type") == "get_player_transform" else nested.get("player_transform", {})
            if isinstance(state, dict) and vector(state.get("location")):
                states.append({
                    "step": check["index"], "source": check["type"], "pie_time_seconds": check["pie_time_seconds"],
                    "location_cm": vector(state["location"]), "rotation": state.get("rotation"),
                    "speed_cm_s": state.get("velocity", {}).get("speed", state.get("speed")),
                    "capsule_half_height_cm": state.get("capsule_half_height"),
                    "camera_location_cm": vector(state.get("camera_location")),
                })
            for prop in data.get("properties", []):
                properties.append({"step": check["index"], "pie_time_seconds": check["pie_time_seconds"], "actor": data.get("actor_name"), "component": data.get("component"), **prop})
                if prop.get("name") == "MovementMode":
                    modes.append({"step": check["index"], "pie_time_seconds": check["pie_time_seconds"], "found": prop.get("found") is True, "value": prop.get("value")})
            screenshot = nested.get("screenshot_path", nested.get("filepath"))
            if screenshot:
                screenshots.append({
                    "step": check["index"], "path": screenshot, "exists": Path(screenshot).exists(),
                    "pie_time_seconds": check["pie_time_seconds"], "frame_number": nested.get("frame_number"),
                    "width": nested.get("width"), "height": nested.get("height"),
                    "mean_luma": nested.get("mean_luma"), "pct_near_black": nested.get("pct_near_black"),
                })
            if check["type"] in ("set_player_transform", "simulate_key_press", "wait_pie_seconds"):
                commands.append({"step": check["index"], "type": check["type"], "pie_time_seconds": check["pie_time_seconds"], "result_data": data})
    if actions:
        if len(actions) != len(steps):
            errors.append(f"Request has {len(actions)} actions but result has {len(steps)} steps.")
        for action, step in zip(actions, steps):
            if action.get("type") != step["type"]:
                errors.append(f"Step {step['index']}: request/result type mismatch.")
    times = [step["pie_time_seconds"] for step in steps if isinstance(step["pie_time_seconds"], (int, float))]
    result.update({
        "status": "execution_pass" if not errors else ("incomplete" if not sequences else "failed"), "execution_pass": not errors,
        "sequence_count": len(sequences), "step_count": len(steps), "step_checks": steps,
        "request_file": str(request_path) if request_path.exists() else None,
        "request_actions": actions, "player_states": states, "movement_modes": modes,
        "screenshots": screenshots, "commands": commands, "actor_properties": properties, "act_windows_pie_seconds": windows,
        "first_observed_pie_time_seconds": min(times) if times else None,
        "last_observed_pie_time_seconds": max(times) if times else None, "errors": errors,
    })
    return result


def ride_checks(test, direction):
    states = [s for s in test.get("player_states", []) if s["source"] == "get_player_transform"]
    if len(states) < 3:
        return {"pass": False, "checks": [{"name": "three measured transform states", "pass": False}], "reason": "Need settled start, arrival, and post-input departure states."}
    start, arrival, departure = states[0], states[-2], states[-1]
    a, b, c = start["location_cm"], arrival["location_cm"], departure["location_cm"]
    up = direction == "up"
    expected_x = -100 if up else 100
    expected_z = [98.15, 548.15, 542.15] if up else [548.15, 98.15, 92.15]
    checks = []
    def add(name, value, passed, expected):
        checks.append({"name": name, "measured": value, "expected": expected, "pass": bool(passed)})
    for label, point, z in zip(("settled start", "arrival", "walk-off departure"), (a, b, c), expected_z):
        add(f"{label} X", point[0], abs(point[0] - expected_x) <= 5, {"x_cm": expected_x, "tolerance_cm": 5})
        add(f"{label} capsule-centre Z", point[2], abs(point[2] - z) <= 3, {"z_cm": z, "tolerance_cm": 3})
    rise = b[2] - a[2]
    expected_rise = 450 if up else -450
    add("vertical transport", rise, abs(rise - expected_rise) <= 5, {"delta_z_cm": expected_rise, "tolerance_cm": 5})
    along = (b[1] - a[1]) * (1 if up else -1)
    add("no-input transport along run", along, along > 800, {"signed_delta_y_cm_min": 800})
    walkoff = (c[1] - b[1]) * (1 if up else -1)
    add("walk off landing", walkoff, walkoff > 75, {"signed_delta_y_cm_min": 75})
    modes = test.get("movement_modes", [])
    add("settled movement mode", modes, any(m["found"] and m["value"] == "MOVE_Walking" for m in modes), "MOVE_Walking")
    # The corrected downhill setup must never snap to the roof above the gallery.
    if not up:
        transforms = [a for a in test.get("request_actions", []) if a.get("type") == "set_player_transform"]
        snap = transforms[0].get("params", {}).get("snap_to_ground") if transforms else None
        add("downhill setup ground snapping", snap, snap is False, False)
    return {"pass": all(c["pass"] for c in checks), "checks": checks, "settled_start": start, "arrival": arrival, "walk_off_departure": departure, "rise_cm": rise, "transport_duration_seconds": arrival["pie_time_seconds"] - start["pie_time_seconds"] if arrival["pie_time_seconds"] is not None and start["pie_time_seconds"] is not None else None, "note": "Position tolerances are explicit physical sanity checks; visual quality and full circulation still need review."}


def store_checks(test):
    actions = test.get("request_actions", [])
    states = [s for s in test.get("player_states", []) if s["source"] == "get_player_transform"]
    entries, identifiers = [], set()
    for state in states:
        setup = next((a.get("params", {}) for a in reversed(actions[:state["step"]]) if a.get("type") == "set_player_transform"), None)
        if setup is None:
            entries.append({"step": state["step"], "pass": False, "error": "No preceding saved setup action."})
            continue
        point = state["location_cm"]
        side = "W" if setup["x"] < 0 else "E"
        floor = 0 if setup["z"] < 300 else 450
        identifier = f"{side}_Y{setup['y']}_Floor{floor}"
        identifiers.add(identifier)
        checks = {
            "inside_storefront": point[0] <= -1250 if side == "W" else point[0] >= 1250,
            "bay_centre_y": abs(point[1] - setup["y"]) <= 5,
            "floor_contact_z": abs(point[2] - (floor + 92.15)) <= 3,
            "setup_did_not_snap_to_roof": setup.get("snap_to_ground") is False,
        }
        entries.append({"bay": identifier, "step": state["step"], "pie_time_seconds": state["pie_time_seconds"], "setup_location_cm": [setup[k] for k in ("x", "y", "z")], "measured_location_cm": point, "signed_entry_distance_cm": abs(point[0]) - abs(setup["x"]), "expected": {"storefront_x_cm": -1200 if side == "W" else 1200, "player_centre_at_least_50cm_inside": True, "y_cm": setup["y"], "capsule_centre_z_cm": floor + 92.15}, "checks": checks, "pass": all(checks.values())})
    expected_ids = {f"{side}_Y{y}_Floor{floor}" for side in ("W", "E") for y in (-1200, 0, 1200) for floor in (0, 450)}
    coverage = identifiers == expected_ids and len(entries) == 12
    return {"pass": coverage and all(e["pass"] for e in entries), "expected_bays": 12, "measured_entries": len(entries), "coverage_pass": coverage, "missing_bays": sorted(expected_ids - identifiers), "unexpected_bays": sorted(identifiers - expected_ids), "entries": entries, "note": "Checks entry across X±1200 storefronts and ground/gallery floor height. The saved request teleports to each approach, then uses movement input; it does not establish a continuous route between all stores."}


def gallery_checks(test):
    expected = [[900, -1900, 542.15], [900, 1900, 542.15], [-900, 1900, 542.15], [-900, -1900, 542.15], [900, -1900, 542.15]]
    states = [s for s in test.get("player_states", []) if s["source"] == "get_player_transform"]
    positioning = [a for a in test.get("request_actions", []) if a.get("type") == "set_player_transform" and any(k in a.get("params", {}) for k in ("x", "y", "z"))]
    checks = [{"name": "five measured route states", "pass": len(states) == 5, "measured": len(states), "expected": 5}, {"name": "one initial placement with no subsequent location teleports", "pass": len(positioning) == 1, "measured": len(positioning), "expected": 1}]
    corners = []
    for state, point in zip(states, expected):
        actual = state["location_cm"]
        passed = abs(actual[0] - point[0]) <= 100 and abs(actual[1] - point[1]) <= 100 and abs(actual[2] - point[2]) <= 3
        corners.append({"step": state["step"], "pie_time_seconds": state["pie_time_seconds"], "location_cm": actual, "expected_location_cm": point, "xy_tolerance_cm": 100, "z_tolerance_cm": 3, "pass": passed})
    modes = test.get("movement_modes", [])
    checks.append({"name": "movement mode remains walking", "pass": any(m["found"] and m["value"] == "MOVE_Walking" for m in modes), "measured": modes, "expected": "MOVE_Walking"})
    return {"pass": all(c["pass"] for c in checks) and all(c["pass"] for c in corners), "checks": checks, "route_states": corners, "note": "Checks the four-side continuous walking circuit around the atrium with yaw-only turns after initial placement. Appearance still requires screenshot review."}


def guard_checks(test):
    states = [s for s in test.get("player_states", []) if s["source"] == "get_player_transform"]
    checks = []
    expected = [("east", 0, 800, 1), ("west", 0, -800, -1), ("south", 1, -1600, -1), ("north", 1, 1600, 1)]
    for state, (label, axis, boundary, sign) in zip(states, expected):
        point = state["location_cm"]
        checks.append({"side": label, "step": state["step"], "pie_time_seconds": state["pie_time_seconds"], "location_cm": point, "void_boundary_cm": boundary, "outward_clearance_cm": (point[axis] - boundary) * sign, "pass": (point[axis] - boundary) * sign >= 0 and abs(point[2] - 542.15) <= 3})
    return {"pass": len(states) == 4 and all(c["pass"] for c in checks), "expected_sides": 4, "measured_sides": len(states), "checks": checks, "note": "Player movement directed into the void must stop outside each boundary and remain on the gallery. These are four sampled guard locations, not exhaustive testing of every join."}


def fountain_capture_checks(test):
    frames = test.get("screenshots", [])
    times = [f["pie_time_seconds"] for f in frames if isinstance(f["pie_time_seconds"], (int, float))]
    checks = {"two_existing_images": len(frames) == 2 and all(f["exists"] for f in frames), "distinct_live_frames": len({f.get("frame_number") for f in frames}) == 2, "later_frame_at_least_one_second_apart": len(times) == 2 and times[1] - times[0] >= 1, "nonblack_images": len(frames) == 2 and all(f.get("mean_luma", 0) > 0 and f.get("pct_near_black", 1) < 0.99 for f in frames)}
    return {"pass": all(checks.values()), "checks": checks, "separation_pie_seconds": times[1] - times[0] if len(times) == 2 else None, "note": "Confirms two live rendered captures exist. Water animation/appearance requires image review; audio is not tested by captures."}


def saved_scalar(text, key):
    """Read a named scalar from the saved JSON or simple YAML tool response."""
    try:
        decoded = json.loads(text)
    except json.JSONDecodeError:
        match = re.search(r"^\s*" + re.escape(key) + r":\s*(.*?)\s*$", text, re.MULTILINE)
        if not match:
            return None
        value = match.group(1)
        if value in ("true", "false"):
            return value == "true"
        try:
            return float(value) if "." in value else int(value)
        except ValueError:
            return value
    if key in decoded:
        return decoded[key]
    return decoded.get("data", {}).get(key)


def stop_checks(path):
    if not path.exists():
        return {"pass": False, "status": "pending", "file": str(path)}
    try:
        calls = json.loads(path.read_text(encoding="utf-8-sig"))
    except (UnicodeError, json.JSONDecodeError) as exc:
        return {"pass": False, "status": "invalid", "error": str(exc), "file": str(path)}
    if isinstance(calls, dict):
        calls = calls.get("calls", [])
    parsed = {}
    for call in calls:
        tool_result = call.get("result", {})
        response = "\n".join(c.get("text", "") for c in tool_result.get("content", []) if c.get("type") == "text")
        fields = {k: saved_scalar(response, k) for k in ("success", "status", "is_active", "pie_time_seconds", "stopped_reason", "capture_mode", "video_path", "index_path", "frame_count", "duration_seconds", "time_span_seconds")}
        fields["isError"] = tool_result.get("isError")
        parsed[call.get("tool")] = fields
    stopped = parsed.get("stop_pie", {})
    inactive = parsed.get("is_pie_active", {})
    recording = parsed.get("stop_pie_recording", {})
    passed = all(p.get("success") is True and p.get("isError") is False for p in (stopped, inactive, recording)) and inactive.get("is_active") is False and recording.get("status") == "recording_stopped"
    return {"pass": passed, "status": "confirmed" if passed else "failed", "file": str(path), "pie_stop_time_seconds": stopped.get("pie_time_seconds"), "is_pie_active": inactive.get("is_active"), "tool_results": parsed, "note": "Recording timestamps are mapped through recording_index.json; video-clock seconds are not PIE-clock seconds."}


def main():
    project = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument("--project", type=Path, default=project)
    args = parser.parse_args()
    source = args.project / "Automation" / "Mall"
    tests = {name: summarize_test(source / filename, source / "gallery_final_verify_request.json" if name == "gallery_loop" else None) for name, filename in REQUIRED.items()}
    for key, direction in (("up_ride", "up"), ("down_ride", "down")):
        tests[key]["physical_checks"] = ride_checks(tests[key], direction)
    tests["store_entry"]["physical_checks"] = store_checks(tests["store_entry"])
    tests["gallery_loop"]["physical_checks"] = gallery_checks(tests["gallery_loop"])
    tests["gallery_guards"]["physical_checks"] = guard_checks(tests["gallery_guards"])
    tests["fountain_visual"]["capture_checks"] = fountain_capture_checks(tests["fountain_visual"])
    known = set(REQUIRED.values()) | set(EXCLUDED)
    extra = {p.name: summarize_test(p) for p in sorted(source.glob("clean_*verify_result.json")) if p.name not in known}
    visual = source / "gallery_visual_verify_result.json"
    if visual.exists():
        extra[visual.name] = summarize_test(visual)
    diagnostic_gallery = summarize_test(source / "gallery_verify_result.json", source / "gallery_verify_request.json")
    diagnostic_gallery["physical_checks"] = gallery_checks(diagnostic_gallery)
    diagnostic_gallery["acceptance_eligible"] = False
    diagnostic_retest = summarize_test(source / "gallery_corrected_verify_result.json", source / "gallery_verify_request.json")
    diagnostic_retest["physical_checks"] = gallery_checks(diagnostic_retest)
    diagnostic_retest["acceptance_eligible"] = False
    present = all(t["status"] != "pending" for t in tests.values())
    nested_pass = all(t["execution_pass"] for t in tests.values())
    rides_pass = all(tests[key]["physical_checks"]["pass"] for key in ("up_ride", "down_ride"))
    stores_pass = tests["store_entry"]["physical_checks"]["pass"]
    gallery_pass = tests["gallery_loop"]["physical_checks"]["pass"]
    guards_pass = tests["gallery_guards"]["physical_checks"]["pass"]
    fountain_pass = tests["fountain_visual"]["capture_checks"]["pass"]
    stopped = stop_checks(source / "finish_pie_final_result.json")
    confirmation_path = args.project / "Saved" / "MallBuild" / "final_verification_confirmation.json"
    confirmation = json.loads(confirmation_path.read_text(encoding="utf-8")) if confirmation_path.exists() else {}
    evidence_complete = present and nested_pass and rides_pass and stores_pass and gallery_pass and guards_pass and fountain_pass
    final_status = evidence_complete and stopped["pass"] and confirmation.get("agent_visual_review_completed") is True
    summary = {
        "generated_utc": datetime.now(timezone.utc).isoformat(), "level": "/Game/Environment/Mall/Maps/Lvl_MallAtrium_Test",
        "source_directory": str(source), "method": "Offline result-file inspection. Requires outer success, every step.ok, every step.result.success, nonempty step lists, and matching saved request actions when available.",
        "required_result_files_present": present, "all_required_execution_checks_pass": nested_pass,
        "both_ride_physical_checks_pass": rides_pass, "all_twelve_store_entry_physical_checks_pass": stores_pass,
        "continuous_gallery_physical_checks_pass": gallery_pass,
        "sampled_gallery_guard_physical_checks_pass": guards_pass, "fountain_capture_checks_pass": fountain_pass,
        "automated_evidence_complete": evidence_complete,
        "final_status": final_status, "stop_confirmed": stopped["pass"], "pie_stop_verification": stopped,
        "agent_visual_review_completed": confirmation.get("agent_visual_review_completed") is True,
        "visual_review_confirmation": confirmation,
        "user_scale_visual_audio_acceptance": "pending_user_review",
        "tests": tests, "additional_clean_sequences": extra,
        "initial_gallery_failure_diagnostic": diagnostic_gallery,
        "first_gallery_retest_failure_diagnostic": diagnostic_retest,
        "excluded_results": [{"file": str(source / filename), "exists": (source / filename).exists(), "reason": reason} for filename, reason in EXCLUDED.items()],
    }
    destination = args.project / "Saved" / "MallBuild" / "verification_summary.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({"summary_file": str(destination), "automated_evidence_complete": summary["automated_evidence_complete"], "tests": {name: {"status": test["status"], "steps": test.get("step_count"), "errors": test.get("errors", []), "physical_pass": test.get("physical_checks", {}).get("pass")} for name, test in tests.items()}}, indent=2))


if __name__ == "__main__":
    main()
