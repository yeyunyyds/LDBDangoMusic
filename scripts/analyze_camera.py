"""Summarize raw diagnostic matrices; never certifies a gameplay camera."""
import argparse
import json
import math
from collections import defaultdict


def inverse(matrix):
    if len(matrix) != 16 or any(x is None or not math.isfinite(x) for x in matrix):
        return None
    rows = [[float(matrix[r * 4 + c]) for c in range(4)] + [float(r == c) for c in range(4)] for r in range(4)]
    for c in range(4):
        pivot = max(range(c, 4), key=lambda r: abs(rows[r][c]))
        if abs(rows[pivot][c]) < 1e-10:
            return None
        rows[c], rows[pivot] = rows[pivot], rows[c]
        divisor = rows[c][c]
        rows[c] = [x / divisor for x in rows[c]]
        for r in range(4):
            if r != c:
                factor = rows[r][c]
                rows[r] = [a - factor * b for a, b in zip(rows[r], rows[c])]
    return [row[4:] for row in rows]


def candidate(record):
    v, p = record['view'], record['projection']
    if record['view_hresult'] < 0 or record['projection_hresult'] < 0:
        return None
    inv = inverse(v)
    if inv is None or record.get('view_identity'):
        return None
    # Candidate rigid affine view in D3D row-vector convention.
    if any(abs(v[i]) > 1e-3 for i in (3, 7, 11)) or abs(v[15] - 1) > 1e-3:
        return None
    basis = [inv[i][:3] for i in range(3)]
    for i in range(3):
        for j in range(3):
            if abs(sum(a * b for a, b in zip(basis[i], basis[j])) - (i == j)) > .02:
                return None
    result = dict(position=inv[3][:3], basis_x=basis[0], basis_y=basis[1], basis_z=basis[2], confirmed=False)
    if all(x is not None and math.isfinite(x) for x in p) and abs(p[15]) < 1e-4 and abs(abs(p[11]) - 1) < .02 and abs(p[0]) > 1e-6 and abs(p[5]) > 1e-6:
        result['candidate_fov_y_degrees'] = math.degrees(2 * math.atan(1 / abs(p[5])))
        result['candidate_aspect'] = abs(p[5] / p[0])
    return result


def summarize(records):
    groups = defaultdict(list)
    for record in records:
        groups[(record['device_generation'], record['chain_id'], record['present_path'])].append(record)
    output = []
    for (generation, chain, path), rows in groups.items():
        changes = sum(any(a is None or b is None or abs(a - b) > 1e-4 for a, b in zip(before['view'], after['view'])) for before, after in zip(rows, rows[1:]))
        candidates = [value for row in rows if (value := candidate(row)) is not None]
        output.append(dict(generation=generation, chain_id=chain, path=path, records=len(rows),
                           first_tick_ms=rows[0]['tick_ms'], last_tick_ms=rows[-1]['tick_ms'],
                           changed_view_samples=changes, identity_views=sum(bool(r.get('view_identity')) for r in rows),
                           plausible_rigid_views=len(candidates), last_candidate=candidates[-1] if candidates else None,
                           confirmed=False))
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('jsonl')
    args = parser.parse_args()
    with open(args.jsonl, encoding='utf-8') as source:
        records = [json.loads(line) for line in source if line.strip()]
    print(json.dumps(summarize(records), indent=2, ensure_ascii=False))
