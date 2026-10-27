"""Reconcile completed open-gallery acceptance into its saved construction records."""
import json
from pathlib import Path

project = Path(r'C:\unreal\uprj\MyProject2')
checks = project / 'Saved/MallOpenHallwaysVerify'
checkpoint = project / 'Saved/Aura/LDA/District/MallOvalTest'
evidence = json.loads((checks / 'actual_movement_evidence.json').read_text())
audit = json.loads((checks / 'final_scene_audit.json').read_text())
film = json.loads((checks / 'final_recording_summary.json').read_text())
assert evidence['summary']['route_segments_passed'] == 50
assert evidence['summary']['upper_doorway_passed'] == 37
assert evidence['summary']['guard_passed'] == 8
assert not audit['unaffected_actor_changes'] and not audit['dirty_maps'] and not audit['dirty_content']
acceptance = dict(evidence['summary'], status='passed_targeted_async_PIE_acceptance',
                  evidence='Saved/MallOpenHallwaysVerify/actual_movement_evidence.json',
                  ground_doorway_note='29 ground links were not rerun; their geometry is preserved and previous results remain historical',
                  visual_visibility='Actual game captures show both directions with roof and floors visible',
                  escalators='Both no-input rides, walk-offs and continuous gallery/landing connections passed')
report = json.loads((checkpoint / 'verification_summary.json').read_text())
report['pre_acceptance_aura_unaffected_actor_changes'] = report.get('unaffected_actor_changes', [])
report.update(saved=True, unaffected_actor_changes=[], preservation_audit=audit, pie_validation=acceptance)
report['preservation_audit_note'] = 'Final numeric native readback supersedes the earlier stale/string-based changed-actor list.'
(checkpoint / 'verification_summary.json').write_text(json.dumps(report, indent=2) + '\n')
manifest = json.loads((checkpoint / 'connectivity_waypoints.json').read_text())
manifest['pie_validation'] = acceptance
(checkpoint / 'connectivity_waypoints.json').write_text(json.dumps(manifest, indent=2) + '\n')
evidence['final_recording_collection_result'] = evidence.pop('recording_summary', '')
evidence['recording'] = film
evidence['recording_note'] = 'The main film reached its duration cap and was recovered by the final sequence. The last descent is covered by stamped PNGs and numeric reads; the final stop call had no remaining recording to collect.'
(checks / 'actual_movement_evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
print(json.dumps({'records_updated': True, 'map_saved_and_clean': True, 'acceptance': acceptance}))
