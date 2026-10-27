"""Prepare offline PIE portal requests after the final layout is confirmed stable.

This script does not connect to Unreal, start PIE, or execute the requests. Run it
with --confirmed-final only after Aura has stopped rewriting the waypoint file.
"""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import sys


PROJECT = Path(__file__).resolve().parents[2]
DEFAULT_INPUT = PROJECT / "Saved/Aura/LDA/District/MallOvalTest/connectivity_waypoints.json"
DEFAULT_OUTPUT = PROJECT / "Automation/MallRedesign/PortalChecks"
MAX_ACTIONS = 50
MAX_WAIT_SECONDS = 60.0
ACTIONS_PER_LINK = 4


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


def ceil_milliseconds(seconds: float) -> float:
    return math.ceil(seconds * 1000.0) / 1000.0


def prepare_link(index: int, link: dict, dataset: dict, speed: float, allowance: float,
                 settle: float) -> dict:
    prefix = f"links[{index}]"
    if not isinstance(link, dict):
        raise ValueError(f"{prefix} must be an object")
    waypoints = link.get("waypoints")
    if not isinstance(waypoints, list) or len(waypoints) < 2:
        raise ValueError(f"{prefix}.waypoints needs at least an approach and destination")
    points = [point3(p, f"{prefix}.waypoints[{i}]") for i, p in enumerate(waypoints)]
    start, end = points[0], points[-1]
    dx, dy = end[0] - start[0], end[1] - start[1]
    distance = math.hypot(dx, dy)
    if distance <= 0.01:
        raise ValueError(f"{prefix} has no horizontal travel distance")
    direction = [dx / distance, dy / distance]
    for i, point in enumerate(points):
        along = (point[0] - start[0]) * direction[0] + (point[1] - start[1]) * direction[1]
        lateral = abs((point[0] - start[0]) * -direction[1] + (point[1] - start[1]) * direction[0])
        if lateral > 1.0 or along < -1.0 or along > distance + 1.0:
            raise ValueError(f"{prefix} is not a straight portal route; split it into straight links")
        if abs(point[2] - start[2]) > 0.5:
            raise ValueError(f"{prefix} changes height; this generator checks same-floor passages")

    floor = link.get("floor")
    floor_heights = dataset.get("floor_z_cm")
    if isinstance(floor, bool) or not isinstance(floor, int):
        raise ValueError(f"{prefix}.floor must be an integer floor index")
    if not isinstance(floor_heights, list) or not 0 <= floor < len(floor_heights):
        raise ValueError(f"{prefix}.floor does not identify a recorded floor_z_cm value")
    floor_z = finite_number(floor_heights[floor], f"floor_z_cm[{floor}]")
    for field in ("from", "to", "kind"):
        if not isinstance(link.get(field), str) or not link[field].strip():
            raise ValueError(f"{prefix}.{field} must be a nonempty string")

    # The current three-point portal format records the opening plane at its
    # middle waypoint. For a two-point route the midpoint is explicitly inferred.
    if len(points) % 2 == 1:
        plane_point = points[len(points) // 2]
        plane_source = "middle recorded waypoint"
    else:
        plane_point = [(a + b) / 2.0 for a, b in zip(start, end)]
        plane_source = "inferred midpoint between approach and destination"
    plane_progress = ((plane_point[0] - start[0]) * direction[0]
                      + (plane_point[1] - start[1]) * direction[1])
    if not 0.0 < plane_progress < distance:
        raise ValueError(f"{prefix} does not approach and finish on opposite sides of the opening plane")

    yaw = math.degrees(math.atan2(dy, dx))
    press = ceil_milliseconds(distance / speed + allowance)
    wait = ceil_milliseconds(press + settle)
    if wait > MAX_WAIT_SECONDS:
        raise ValueError(f"{prefix} needs {wait:.3f}s wait; split routes exceeding 60s")
    actions = [
        {"type": "set_player_transform", "params": {
            "x": start[0], "y": start[1], "z": start[2],
            "yaw": yaw, "pitch": 0, "rotation_target": "both", "snap_to_ground": False,
        }},
        {"type": "simulate_key_press", "params": {"key": "W", "press_duration": press}},
        {"type": "wait_pie_seconds", "params": {"seconds": wait}},
        {"type": "get_player_transform", "params": {}},
    ]
    radius = finite_number(dataset.get("capsule_radius_cm"), "capsule_radius_cm")
    expected = {
        "link_index_0_based": index,
        "source": link["from"], "destination": link["to"],
        "floor": floor, "kind": link["kind"],
        "approach_capsule_xyz_cm": start,
        "expected_destination_capsule_xyz_cm": end,
        "recorded_waypoints_xyz_cm": points,
        "floor_surface_z_cm": floor_z,
        "expected_capsule_center_z_cm": end[2],
        "expected_capsule_center_above_floor_cm": end[2] - floor_z,
        "opening_width_cm": link.get("opening_width_cm"),
        "opening_height_cm": link.get("opening_height_cm"),
        "opening_plane": {
            "point_xyz_cm": plane_point,
            "normal_unit_xy_toward_destination": direction,
            "source": plane_source,
            "approach_signed_distance_cm": -plane_progress,
            "expected_destination_signed_distance_cm": distance - plane_progress,
        },
        "horizontal_distance_cm": distance,
        "yaw_degrees": yaw,
        "recorded_max_walk_speed_cm_s": speed,
        "acceleration_allowance_seconds": allowance,
        "press_duration_seconds": press,
        "wait_seconds": wait,
        "full_capsule_clearance_progress_cm": plane_progress + radius,
        "validation_guidance": {
            "require_all_nested_actions_success": True,
            "measure_actual_displacement_from_setter_result_when_available": True,
            "require_opening_plane_crossed_toward_destination": True,
            "require_capsule_center_height_matches_recorded_destination": True,
            "same_floor_capsule_z_tolerance_cm": 5.0,
            "destination_xy_tolerance_cm": max(50.0, speed * allowance + 20.0),
            "lateral_route_tolerance_cm": 35.0,
        },
        "status": "prepared_not_executed",
    }
    return {"actions": actions, "expected": expected, "wait": wait}


def build_requests(dataset: dict, allowance: float, settle: float) -> tuple[list[dict], list[dict]]:
    speed = finite_number(dataset.get("max_walk_speed_cm_s"), "max_walk_speed_cm_s")
    if speed <= 0:
        raise ValueError("Recorded max_walk_speed_cm_s must be greater than zero")
    links = dataset.get("links")
    if not isinstance(links, list) or not links:
        raise ValueError("Input needs a nonempty links array")
    requests, batches = [], []
    actions, checks, wait_total = [], [], 0.0

    def finish_batch() -> None:
        nonlocal actions, checks, wait_total
        if not actions:
            return
        batch_index = len(requests) + 1
        filename = f"portal_checks_{batch_index:03d}_request.json"
        requests.append({"tool": "run_verification_sequence", "arguments": {
            "actions": actions, "record": True, "attach_images": False,
        }})
        batches.append({
            "batch_index_1_based": batch_index, "request_file": filename,
            "action_count": len(actions), "summed_wait_seconds": round(wait_total, 6),
            "checks": checks,
        })
        actions, checks, wait_total = [], [], 0.0

    for index, link in enumerate(links):
        item = prepare_link(index, link, dataset, speed, allowance, settle)
        if (len(actions) + ACTIONS_PER_LINK > MAX_ACTIONS
                or wait_total + item["wait"] > MAX_WAIT_SECONDS + 1e-9):
            finish_batch()
        step = len(actions)
        expected = item["expected"]
        expected["request_step_indices_0_based"] = {
            "set_player_transform": step, "simulate_key_press": step + 1,
            "wait_pie_seconds": step + 2, "get_player_transform": step + 3,
        }
        expected["request_step_numbers_1_based"] = {
            name: value + 1 for name, value in expected["request_step_indices_0_based"].items()
        }
        actions.extend(item["actions"])
        checks.append(expected)
        wait_total += item["wait"]
    finish_batch()
    return requests, batches


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--confirmed-final", action="store_true",
                        help="Confirm that Aura has finished and the final waypoint file is stable")
    parser.add_argument("--acceleration-allowance", type=float, default=0.15,
                        help="Extra W input seconds beyond distance / recorded walking speed (default .15)")
    parser.add_argument("--settle-seconds", type=float, default=0.25,
                        help="Wait beyond input duration, at least .25 seconds (default .25)")
    args = parser.parse_args()
    if not args.confirmed_final:
        parser.error("No requests written. Run with --confirmed-final only after the layout is stable.")
    if not math.isfinite(args.acceleration_allowance) or not 0 <= args.acceleration_allowance <= 1:
        parser.error("--acceleration-allowance must be between zero and one second")
    if not math.isfinite(args.settle_seconds) or args.settle_seconds < 0.25:
        parser.error("--settle-seconds must be at least .25 seconds")

    source = args.input.resolve()
    raw = source.read_bytes()
    source_sha = hashlib.sha256(raw).hexdigest()
    dataset = json.loads(raw.decode("utf-8-sig"))
    if not isinstance(dataset, dict):
        raise ValueError("Waypoint input must be a JSON object")
    requests, batches = build_requests(dataset, args.acceleration_allowance, args.settle_seconds)
    if source.read_bytes() != raw:
        raise RuntimeError("Waypoint input changed during preparation; no requests written")

    output = args.output_dir.resolve() / source_sha[:12]
    manifest = {
        "status": "prepared_not_executed", "map": dataset.get("map"),
        "source_file": str(source), "source_sha256": source_sha,
        "confirmation": "Generator invoked with --confirmed-final",
        "direction": "Recorded approach to recorded destination; no reverse tests implied",
        "capsule_radius_cm": dataset.get("capsule_radius_cm"),
        "capsule_halfheight_cm": dataset.get("capsule_halfheight_cm"),
        "floor_z_cm": dataset.get("floor_z_cm"),
        "recorded_max_walk_speed_cm_s": dataset["max_walk_speed_cm_s"],
        "total_links": len(dataset["links"]), "total_batches": len(batches),
        "counts_by_kind": dict(Counter(link["kind"] for link in dataset["links"])),
        "limits": {"maximum_actions": MAX_ACTIONS, "maximum_summed_wait_seconds": MAX_WAIT_SECONDS},
        "batches": batches,
    }
    output.mkdir(parents=True, exist_ok=True)
    for request, batch in zip(requests, batches):
        target = output / batch["request_file"]
        target.write_text(json.dumps(request, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    manifest_path = output / "portal_checks_expected.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    if source.read_bytes() != raw:
        raise RuntimeError(f"Waypoint input changed while writing. Do not execute requests in {output}.")
    print(json.dumps({"status": "prepared_not_executed", "requests_directory": str(output),
                      "expected_manifest": str(manifest_path), "links": len(dataset["links"]),
                      "batches": len(batches)}, indent=2))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError, KeyError) as error:
        print(f"Preparation failed: {error}", file=sys.stderr)
        sys.exit(1)
