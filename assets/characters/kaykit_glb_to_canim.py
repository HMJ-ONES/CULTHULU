#!/usr/bin/env python3
"""KayKit Rig_Medium GLB animations -> CULT-ULHU .canim converter (one-shot).

Reads glTF animation channels, resamples translation+rotation onto a union
timeline per bone, converts positions to rest-pose offsets and quaternions to
XYZ euler degrees (matching BoneTrack.h: rotEuler in degrees, XYZ order),
then writes the .canim text format ClipSerializer::load() parses:

    CLIP "name" <durationSeconds> <loop 0|1>
    TRACK <bone> <nkeys>
    KEY <time> <px> <py> <pz> <rx> <ry> <rz>
"""
import struct, json, math, sys, os

COMP = {5120: ('b',1), 5121: ('B',1), 5122: ('h',2), 5123: ('H',2),
        5125: ('I',4), 5126: ('f',4)}
TYPESZ = {'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}

def load_glb(path):
    d = open(path,'rb').read()
    assert d[:4] == b'glTF', path
    jl = struct.unpack('<I', d[12:16])[0]
    js = json.loads(d[20:20+jl])
    bo = 20 + jl
    bl = struct.unpack('<I', d[bo:bo+4])[0]
    assert d[bo+4:bo+8] == b'BIN\x00'
    return js, d[bo+8:bo+8+bl]

def read_accessor(js, binc, idx):
    acc = js['accessors'][idx]
    bv = js['bufferViews'][acc['bufferView']]
    fmt, sz = COMP[acc['componentType']]
    n = TYPESZ[acc['type']]
    off = bv.get('byteOffset',0) + acc.get('byteOffset',0)
    stride = bv.get('byteStride', n*sz)
    out = []
    for i in range(acc['count']):
        vals = struct.unpack_from('<'+fmt*n, binc, off+i*stride)
        out.append(vals[0] if n==1 else vals)
    return out

# ---- math ----
def qnormalize(q):
    n = math.sqrt(sum(v*v for v in q)) or 1.0
    return tuple(v/n for v in q)

def qmat(q):
    x,y,z,w = q
    return (
        (1-2*(y*y+z*z), 2*(x*y-z*w),   2*(x*z+y*w)),
        (2*(x*y+z*w),   1-2*(x*x+z*z), 2*(y*z-x*w)),
        (2*(x*z-y*w),   2*(y*z+x*w),   1-2*(x*x+y*y)),
    )

def quat_to_euler_xyz(q):
    """R = Rx(x)Ry(y)Rz(z); returns degrees. Verified by reconstruction."""
    m = qmat(qnormalize(q))
    sy = max(-1.0, min(1.0, m[0][2]))
    y = math.asin(sy)
    if abs(abs(sy)-1.0) > 1e-6:
        x = math.atan2(-m[1][2], m[2][2])
        z = math.atan2(-m[0][1], m[0][0])
    else:
        s = 1.0 if sy > 0 else -1.0
        x = s*math.atan2(m[1][0], m[1][1]); z = 0.0
    return (math.degrees(x), math.degrees(y), math.degrees(z))

def euler_xyz_to_mat(e):
    x,y,z = (math.radians(v) for v in e)
    cx,sx, cy,sy, cz,sz = math.cos(x),math.sin(x),math.cos(y),math.sin(y),math.cos(z),math.sin(z)
    Rx=((1,0,0),(0,cx,-sx),(0,sx,cx)); Ry=((cy,0,sy),(0,1,0),(-sy,0,cy)); Rz=((cz,-sz,0),(sz,cz,0),(0,0,1))
    def mm(a,b): return tuple(tuple(sum(a[i][k]*b[k][j] for k in range(3)) for j in range(3)) for i in range(3))
    return mm(Rx, mm(Ry, Rz))

def slerp(a,b,t):
    a,b = qnormalize(a), qnormalize(b)
    d = sum(p*q for p,q in zip(a,b))
    if d < 0: b = tuple(-v for v in b); d = -d
    if d > 0.9995:
        r = tuple(p+(q-p)*t for p,q in zip(a,b)); return qnormalize(r)
    th = math.acos(d); s = math.sin(th)
    k0, k1 = math.sin((1-t)*th)/s, math.sin(t*th)/s
    return tuple(p*k0+q*k1 for p,q in zip(a,b))

def lerp(a,b,t): return tuple(p+(q-p)*t for p,q in zip(a,b))

def sample_track(keys, t, is_quat):
    if t <= keys[0][0]: return keys[0][1]
    if t >= keys[-1][0]: return keys[-1][1]
    for i in range(1,len(keys)):
        if t <= keys[i][0]:
            t0,v0 = keys[i-1]; t1,v1 = keys[i]
            f = (t-t0)/(t1-t0) if t1>t0 else 0.0
            return slerp(v0,v1,f) if is_quat else lerp(v0,v1,f)
    return keys[-1][1]

def convert_clip(glb_path, clip_name):
    js, binc = load_glb(glb_path)
    nodes = js['nodes']
    anim = next(a for a in js['animations'] if a.get('name')==clip_name)
    rest = {}
    for j in js['skins'][0]['joints']:
        rest[nodes[j].get('name','?')] = tuple(nodes[j].get('translation',[0,0,0]))
    chans = {}
    for ch in anim['channels']:
        tgt = ch['target']; ni = tgt['node']; path = tgt['path']
        nm = nodes[ni].get('name','?')
        s = anim['samplers'][ch['sampler']]
        times = read_accessor(js,binc,s['input'])
        vals  = read_accessor(js,binc,s['output'])
        chans.setdefault(nm, {})[path] = list(zip(times, vals))
    tracks = {}
    scale_warn = []
    for bone, chd in chans.items():
        times = sorted({t for path in chd.values() for t,_ in path})
        tkeys = chd.get('translation'); rkeys = chd.get('rotation'); skeys = chd.get('scale')
        if skeys and any(abs(v-1.0)>1e-3 for _,sv in skeys for v in sv):
            scale_warn.append(bone)
        keys = []
        max_err = 0.0
        for t in times:
            p = sample_track(tkeys,t,False) if tkeys else rest[bone]
            q = sample_track(rkeys,t,True) if rkeys else (0,0,0,1)
            e = quat_to_euler_xyz(q)
            # verify reconstruction
            m1, m2 = qmat(qnormalize(q)), euler_xyz_to_mat(e)
            err = max(abs(m1[i][j]-m2[i][j]) for i in range(3) for j in range(3))
            max_err = max(max_err, err)
            off = tuple(pi-ri for pi,ri in zip(p, rest[bone]))
            keys.append((t, off, e))
        tracks[bone] = (keys, max_err)
    dur = max(t for bone,(keys,_) in tracks.items() for t,_e in [(k[0],0) for k in keys])
    return tracks, dur, scale_warn

def write_canim(path, clip_label, tracks, duration, loop):
    with open(path,'w') as f:
        f.write('CLIP "%s" %.6g %d\n' % (clip_label, duration, 1 if loop else 0))
        for bone in sorted(tracks):
            keys,_ = tracks[bone]
            f.write('TRACK %s %d\n' % (bone, len(keys)))
            for t, off, e in keys:
                f.write('KEY %.6g %.5g %.5g %.5g %.3f %.3f %.3f\n' %
                        (t, off[0], off[1], off[2], e[0], e[1], e[2]))

def verify_canim(path):
    """Re-implement ClipSerializer::load grammar check."""
    toks = open(path).read().split()
    assert toks[0]=='CLIP', path
    i = 1
    assert toks[i].startswith('"'); i += 1
    # name may contain spaces -> quoted token already single; our names have none
    dur = float(toks[i]); li = int(toks[i+1]); i += 2
    ntracks = nkeys = 0
    while i < len(toks):
        assert toks[i]=='TRACK', (path, i); bone=toks[i+1]; n=int(toks[i+2]); i+=3
        ntracks += 1
        for _ in range(n):
            assert toks[i]=='KEY', (path,i); i+=1
            float(toks[i]); [float(toks[i+1+k]) for k in range(6)]; i+=7
            nkeys += 1
    return dur, li, ntracks, nkeys

if __name__ == '__main__':
    # quick self-test: euler round-trip on random quats
    import random
    worst = 0.0
    for _ in range(2000):
        q = qnormalize((random.uniform(-1,1),random.uniform(-1,1),random.uniform(-1,1),random.uniform(-1,1)))
        e = quat_to_euler_xyz(q)
        m1, m2 = qmat(q), euler_xyz_to_mat(e)
        worst = max(worst, max(abs(m1[i][j]-m2[i][j]) for i in range(3) for j in range(3)))
    print("euler round-trip worst matrix error:", worst)
