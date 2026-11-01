"""Surface-connected anatomical skinning; prevents cross-body heat-weight bleed.

Bone proximity blends joints within each anatomical region. Surface geodesics
separate nearby limbs that are far apart along the actual skin. Four influences
per vertex keep the result suitable for gameplay rather than an offline rig.
"""
import heapq
import math
import numpy as np
from mathutils.kdtree import KDTree


def bind(mesh, rig, groups, seeds, blend_width=.075):
    p = np.array([tuple(v.co) for v in mesh.data.vertices])
    count = len(p)
    edges = np.array([tuple(e.vertices) for e in mesh.data.edges], dtype=np.int32)
    lengths = np.linalg.norm(p[edges[:, 0]]-p[edges[:, 1]], axis=1)
    adjacency = [[] for _ in range(count)]
    for (a, b), distance in zip(edges, lengths):
        adjacency[a].append((int(b), float(distance)))
        adjacency[b].append((int(a), float(distance)))
    tree = KDTree(count)
    for i, xyz in enumerate(p):
        tree.insert(xyz, i)
    tree.balance()
    regions = list(groups)
    distances = []
    for name in regions:
        d = np.full(count, np.inf)
        queue = []
        for seed in seeds[name]:
            _, index, _ = tree.find(seed)
            d[index] = 0
            heapq.heappush(queue, (0., index))
        while queue:
            at, index = heapq.heappop(queue)
            if at > d[index]:
                continue
            for other, cost in adjacency[index]:
                candidate = at+cost
                if candidate < d[other]:
                    d[other] = candidate
                    heapq.heappush(queue, (candidate, other))
        distances.append(d)
    distances = np.stack(distances, axis=1)
    # Disconnected original sculpt fragments still need a region: use their
    # actual bone proximity if no seeded skin component can reach them.
    bones = [b for b in rig.data.bones if b.use_deform]
    proximity = np.empty((count, len(bones)))
    for j, bone in enumerate(bones):
        a, b = np.array(bone.head_local), np.array(bone.tail_local)
        direction = b-a
        t = np.clip(((p-a) @ direction)/(direction @ direction), 0, 1)
        proximity[:, j] = np.linalg.norm(p-(a+t[:, None]*direction), axis=1)
    bone_names = [b.name for b in bones]
    nearest = distances.min(axis=1)
    disconnected = ~np.isfinite(nearest)
    for j, name in enumerate(regions):
        indices = [bone_names.index(b) for b in groups[name]]
        distances[disconnected, j] = proximity[disconnected][:, indices].min(axis=1)
    nearest = distances.min(axis=1)
    region_weights = np.exp(-np.minimum(80, (distances-nearest[:, None])/blend_width))
    region_weights /= region_weights.sum(axis=1)[:, None]
    weights = np.zeros_like(proximity)
    for j, name in enumerate(regions):
        indices = [bone_names.index(b) for b in groups[name]]
        local = (1/(proximity[:, indices]+.025))**4
        local /= local.sum(axis=1)[:, None]
        weights[:, indices] = local*region_weights[:, j, None]
    # Smooth influence transitions along the actual surface before limiting the
    # GPU palette. This does not connect a hand to a nearby thigh through air.
    a, b = edges[:, 0], edges[:, 1]
    conductance = 1/np.maximum(lengths, .001)
    degree = np.bincount(a, weights=conductance, minlength=count)+np.bincount(b, weights=conductance, minlength=count)
    for iteration in range(10):
        previous = weights.copy()
        for j in range(len(bones)):
            total = np.bincount(a, weights=conductance*previous[b, j], minlength=count)+np.bincount(b, weights=conductance*previous[a, j], minlength=count)
            weights[:, j] = .45*previous[:, j]+.55*total/np.maximum(degree, 1e-12)
        weights /= np.maximum(weights.sum(axis=1)[:, None], 1e-12)
    strongest = np.argsort(weights, axis=1)[:, -4:]
    selected = np.take_along_axis(weights, strongest, axis=1)
    selected /= selected.sum(axis=1)[:, None]
    # Dropping tiny influences must precede the final normalization. Otherwise
    # the stored Blender groups can sum below one despite a correct array report.
    selected[selected < .00001] = 0
    selected /= selected.sum(axis=1)[:, None]
    mesh.vertex_groups.clear()
    vertex_groups = [mesh.vertex_groups.new(name=b.name) for b in bones]
    for i in range(count):
        for j, value in zip(strongest[i], selected[i]):
            if value > 0:
                vertex_groups[j].add([i], float(value), 'REPLACE')
    assert np.isfinite(selected).all() and np.allclose(selected.sum(axis=1), 1)
    return {'method': 'surface-geodesic anatomical regions / joint proximity', 'regions': regions,
            'maxInfluences': 4, 'vertices': count, 'disconnectedFallbackVertices': int(disconnected.sum()),
            'surfaceSmoothingIterations': 10,
            'normalizationMaxError': float(np.max(np.abs(selected.sum(axis=1)-1)))}
