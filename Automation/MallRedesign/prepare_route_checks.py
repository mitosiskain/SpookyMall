"""Prepare continuous PIE walking checks from a final connectivity manifest.

No Unreal connection is made and no request is executed. Confirm final manifest
stability before generating requests with --confirmed-final. Each route has one
initial placement. Continuation batches preserve position and must be executed
in order in the same PIE session, with no intervening teleport or movement.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sys

PROJECT = Path(__file__).resolve().parents[2]
DEFAULT_INPUT = PROJECT / "Saved/Aura/LDA/District/MallOvalTest/connectivity_waypoints.json"
DEFAULT_OUTPUT = PROJECT / "Automation/MallRedesign/RouteChecks"
DEFAULT_ROUTE_PATTERN = r"ring|bridge|hall|bypass"
MAX_ACTIONS = 50
MAX_WAIT_SECONDS = 60.0
SETTLE_SECONDS = 0.3
SAME_Z_TOLERANCE_CM = 0.0001


def finite_number(value: object, name: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (float, int)):
        raise ValueError(f"{name} must be a finite number")
    value = float(value)
    if not math.isfinite(value):
        raise ValueError(f"{name} must be a finite number")
    return value


def point3(value: object, name: str) -> list[float]:
    if not isinstance(value, list) or len(value) != 3:
        raise ValueError(f"{name} must contain explicit x, y, z coordinates")
    return [finite_number(v, f"{name}[{i}]") for i, v in enumerate(value)]


def select_routes(dataset: dict, names: list[str] | None, pattern: str) -> tuple[list[dict], list[dict]]:
    routes = dataset.get("routes")
    if not isinstance(routes, list) or not routes:
        raise ValueError("Input must have a nonempty routes array")
    known = set()
    selected, excluded = [], []
    matcher = re.compile(pattern, re.IGNORECASE)
    for index, route in enumerate(routes):
        if not isinstance(route, dict) or not isinstance(route.get("id"), str) or not route["id"].strip():
            raise ValueError(f"routes[{index}] needs a nonempty id")
        identity = route["id"]
        if identity in known:
            raise ValueError(f"Duplicate route id: {identity}")
        known.add(identity)
        requested = identity in names if names else bool(matcher.search(identity))
        exclusion = None
        if "escalator" in identity.lower() or "escalator" in str(route.get("kind", "")).lower():
            exclusion = "Escalator transport is checked separately"
        elif not requested:
            exclusion = "Not selected for this preparation"
        else:
            waypoints = route.get("waypoints")
            if not isinstance(waypoints, list) or len(waypoints) < 2:
                raise ValueError(f"Route {identity} needs at least two waypoints")
            points = [point3(p, f"{identity}.waypoints[{i}]") for i, p in enumerate(waypoints)]
            if any(abs(p[2] - points[0][2]) > SAME_Z_TOLERANCE_CM for p in points):
                exclusion = "Changing-Z route excluded; requires a separate height/transport check"
            else:
                selected.append({**route, "waypoints": points, "source_route_index_0_based": index})
        if exclusion:
            excluded.append({"route": identity, "reason": exclusion, "explicitly_requested": requested})
    if names:
        missing = sorted(set(names) - known)
        if missing:
            raise ValueError(f"Requested route ids not found: {', '.join(missing)}")
    if not selected:
        raise ValueError("No selected same-floor walking routes remain after exclusions")
    return selected, excluded


def build_requests(dataset: dict, routes: list[dict]) -> tuple[list[dict], list[dict]]:
    speed = finite_number(dataset.get("max_walk_speed_cm_s"), "max_walk_speed_cm_s")
    if speed <= 0:
        raise ValueError("Recorded walking speed must be greater than zero")
    floor_heights = dataset.get("floor_z_cm")
    if not isinstance(floor_heights, list):
        raise ValueError("Input must record floor_z_cm")
    requests, batches = [], []

    for route_index, route in enumerate(routes):
        identity = route["id"]
        points = route["waypoints"]
        floor = route.get("floor")
        if isinstance(floor, bool) or not isinstance(floor, int) or not 0 <= floor < len(floor_heights):
            raise ValueError(f"Route {identity} needs a valid floor index")
        floor_z = finite_number(floor_heights[floor], f"floor_z_cm[{floor}]")
        first_dx, first_dy = points[1][0] - points[0][0], points[1][1] - points[0][1]
        first_yaw = math.degrees(math.atan2(first_dy, first_dx))
        start = points[0]
        initial = {"type": "set_player_transform", "params": {
            "x": start[0], "y": start[1], "z": start[2],
            "yaw": first_yaw, "pitch": 0, "rotation_target": "both", "snap_to_ground": False,
        }}
        actions, expected, wait_total = [initial], [], 0.0
        part = 1
        previous_endpoint_ref = None
        previous_batch_index = None

        def finish_batch() -> None:
            nonlocal actions, expected, wait_total, part, previous_batch_index
            if not expected:
                return
            index = len(requests) + 1
            safe_name = re.sub(r"[^A-Za-z0-9_-]+", "_", identity).strip("_")[:80] or "route"
            filename = f"route_checks_{index:03d}_{safe_name}_part_{part:03d}_request.json"
            requests.append({"tool": "run_verification_sequence", "arguments": {
                "actions": actions, "record": True, "attach_images": False,
            }})
            batches.append({
                "batch_index_1_based": index, "request_file": filename,
                "route": identity, "selected_route_index_0_based": route_index,
                "route_part_1_based": part,
                "requires_continuation_from_batch_1_based": previous_batch_index,
                "initial_placement_step_index_0_based": 0 if part == 1 else None,
                "initial_placement_capsule_xyz_cm": start if part == 1 else None,
                "continuation_instruction": (
                    "Start this route with its single explicit placement" if part == 1 else
                    "Continue at the previous batch endpoint in the same PIE session; do not teleport"
                ),
                "action_count": len(actions), "summed_wait_seconds": round(wait_total, 6),
                "segments": expected,
            })
            previous_batch_index = index
            part += 1
            actions, expected, wait_total = [], [], 0.0

        for segment_index, (segment_start, end) in enumerate(zip(points[:-1], points[1:])):
            dx, dy = end[0] - segment_start[0], end[1] - segment_start[1]
            distance = math.hypot(dx, dy)
            if distance <= 0.01:
                raise ValueError(f"Route {identity} segment {segment_index} has duplicate adjacent waypoints")
            yaw = math.degrees(math.atan2(dy, dx))
            press = round(distance / speed, 6)
            wait = math.ceil((press + SETTLE_SECONDS) * 1000.0) / 1000.0
            if wait > MAX_WAIT_SECONDS:
                raise ValueError(f"Route {identity} segment {segment_index} exceeds 60s; add a waypoint")
            if len(actions) + 4 > MAX_ACTIONS or wait_total + wait > MAX_WAIT_SECONDS + 1e-9:
                finish_batch()
            step = len(actions)
            batch_index = len(requests) + 1
            # Every turn omits x/y/z, including after a batch split. This is a
            # rotation change only; the player stays at the actual last endpoint.
            actions.extend([
                {"type": "set_player_transform", "params": {
                    "yaw": yaw, "pitch": 0, "rotation_target": "both", "snap_to_ground": False,
                }},
                {"type": "simulate_key_press", "params": {"key": "W", "press_duration": press}},
                {"type": "wait_pie_seconds", "params": {"seconds": wait}},
                {"type": "get_player_transform", "params": {}},
            ])
            step_map = {"yaw_only_turn": step, "simulate_key_press": step + 1,
                        "wait_pie_seconds": step + 2, "get_player_transform": step + 3}
            expected.append({
                "route": identity, "source_route_index_0_based": route["source_route_index_0_based"],
                "segment_index_0_based": segment_index, "segment_number_1_based": segment_index + 1,
                "floor": floor, "floor_surface_z_cm": floor_z,
                "nominal_start_capsule_xyz_cm": segment_start,
                "expected_end_capsule_xyz_cm": end,
                "expected_capsule_center_z_cm": end[2],
                "horizontal_distance_cm": distance, "direction_unit_xy": [dx / distance, dy / distance],
                "yaw_degrees": yaw, "recorded_max_walk_speed_cm_s": speed,
                "press_duration_seconds": press, "acceleration_allowance_seconds": 0.0,
                "wait_seconds": wait, "post_input_settle_seconds": SETTLE_SECONDS,
                "request_step_indices_0_based": step_map,
                "request_step_numbers_1_based": {name: value + 1 for name, value in step_map.items()},
                "actual_start_reference": previous_endpoint_ref or {
                    "type": "initial_explicit_placement", "capsule_xyz_cm": start,
                    "batch_index_1_based": batch_index, "step_index_0_based": 0,
                },
                "validation_guidance": {
                    "require_all_nested_actions_success": True,
                    "compare_end_to_recorded_target_not_only_action_success": True,
                    "measure_displacement_from_previous_actual_endpoint": True,
                    "require_capsule_height_matches_recorded_floor": True,
                    "same_floor_capsule_z_tolerance_cm": 5.0,
                    "no_teleport_at_batch_continuation": True,
                },
                "status": "prepared_not_executed",
            })
            previous_endpoint_ref = {"type": "get_player_transform_result",
                                     "batch_index_1_based": batch_index, "step_index_0_based": step + 3}
            wait_total += wait
        finish_batch()
    return requests, batches


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--confirmed-final", action="store_true",
                        help="Confirm that Aura is finished and the revised manifest is stable")
    parser.add_argument("--route", action="append", help="Select exact route id; repeat for multiple routes")
    parser.add_argument("--route-regex", default=DEFAULT_ROUTE_PATTERN,
                        help="Selection pattern when --route is absent (default ring|bridge|hall|bypass)")
    args = parser.parse_args()
    if not args.confirmed_final:
        parser.error("No requests written. Run with --confirmed-final only after root confirms stability.")
    source = args.input.resolve()
    raw = source.read_bytes()
    source_sha = hashlib.sha256(raw).hexdigest()
    dataset = json.loads(raw.decode("utf-8-sig"))
    if not isinstance(dataset, dict):
        raise ValueError("Manifest must be a JSON object")
    routes, excluded = select_routes(dataset, args.route, args.route_regex)
    requests, batches = build_requests(dataset, routes)
    if source.read_bytes() != raw:
        raise RuntimeError("Manifest changed during preparation; no requests written")

    selected_ids = [route["id"] for route in routes]
    selection_sha = hashlib.sha256(json.dumps(selected_ids).encode("utf-8")).hexdigest()[:8]
    output = args.output_dir.resolve() / f"{source_sha[:12]}_{selection_sha}"
    manifest = {
        "status": "prepared_not_executed", "map": dataset.get("map"),
        "source_file": str(source), "source_sha256": source_sha,
        "confirmation": "Generator invoked with --confirmed-final",
        "recorded_max_walk_speed_cm_s": dataset["max_walk_speed_cm_s"],
        "selected_routes": selected_ids, "excluded_routes": excluded,
        "total_segments": sum(len(batch["segments"]) for batch in batches),
        "total_batches": len(batches),
        "execution_instruction": (
            "Execute manifest batches in order. Each route gets one initial placement. "
            "Continue split batches in the same PIE session at the actual previous endpoint, "
            "without any intervening teleport or movement. Escalators require separate tests."
        ),
        "limits": {"maximum_actions": MAX_ACTIONS, "maximum_summed_wait_seconds": MAX_WAIT_SECONDS},
        "batches": batches,
    }
    output.mkdir(parents=True, exist_ok=True)
    for request, batch in zip(requests, batches):
        (output / batch["request_file"]).write_text(
            json.dumps(request, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    expected_path = output / "route_checks_expected.json"
    expected_path.write_text(json.dumps(manifest, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    if source.read_bytes() != raw:
        raise RuntimeError(f"Manifest changed while writing. Do not execute requests in {output}.")
    print(json.dumps({"status": "prepared_not_executed", "requests_directory": str(output),
                      "expected_manifest": str(expected_path), "routes": selected_ids,
                      "segments": manifest["total_segments"], "batches": len(batches)}, indent=2))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError, KeyError, re.error) as error:
        print(f"Preparation failed: {error}", file=sys.stderr)
        sys.exit(1)
