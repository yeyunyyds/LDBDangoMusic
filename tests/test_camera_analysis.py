import importlib.util
from pathlib import Path

path = Path(__file__).resolve().parents[1] / 'scripts/analyze_camera.py'
spec = importlib.util.spec_from_file_location('analysis', path)
a = importlib.util.module_from_spec(spec)
spec.loader.exec_module(a)
identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
view = identity.copy()
view[12:15] = [-10, -20, -30]
record = dict(view=view, projection=[1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 1, 0, 0, -1, 0], view_hresult=0, projection_hresult=0,
              view_identity=False, device_generation=1, chain_id=42, present_path='SwapChain', tick_ms=1)
pose = a.candidate(record)
if pose['position'] != [10, 20, 30] or pose['candidate_aspect'] != 2 or pose['confirmed']:
    raise RuntimeError('Incorrect candidate pose')
if a.inverse([0] * 16) is not None or a.inverse([float('nan')] * 16) is not None:
    raise RuntimeError('Accepted invalid matrix')
if a.candidate(dict(record, view_identity=True)) is not None or a.candidate(dict(record, view_hresult=-1)) is not None:
    raise RuntimeError('Accepted identity/failed view')
summary = a.summarize([record, dict(record, view=identity, tick_ms=2), dict(record, chain_id=43)])
if len(summary) != 2 or summary[0]['changed_view_samples'] != 1 or summary[0]['confirmed']:
    raise RuntimeError('Mixed chains or incorrectly confirmed camera')
print('Passed: candidate inverse, invalid matrices, HRESULT rejection, per-chain diagnostic grouping')
