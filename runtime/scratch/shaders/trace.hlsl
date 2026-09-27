struct Triangle {
    float4 v0;
    float4 v1;
    float4 v2;
    float4 n;
    float4 rgb;
};

StructuredBuffer<Triangle> tris : register(t0);

cbuffer Params : register(b0) {
    float4 camera;
    float4 hvFov;
    float4 light;
    int4 gridSampTri;
};

RWStructuredBuffer<uint> outRgba : register(u0);

float3 camDir(float hDeg, float vDeg) {
    const float d = 3.14159265358979323846 / 180.0;
    float h = hDeg * d;
    float v = vDeg * d;
    float3 dir = float3(cos(v) * sin(h), sin(v), cos(v) * cos(h));
    float len = length(dir);
    return len > 1e-8 ? dir / len : dir;
}

bool hitTri(float3 orig, float3 dir, Triangle tri, out float tHit) {
    float3 e1 = tri.v1.xyz - tri.v0.xyz;
    float3 e2 = tri.v2.xyz - tri.v0.xyz;
    float3 p = cross(dir, e2);
    float det = dot(e1, p);
    if (abs(det) < 1e-8) return false;
    float inv = 1.0 / det;
    float3 s = orig - tri.v0.xyz;
    float u = dot(s, p) * inv;
    if (u < 0 || u > 1) return false;
    float3 q = cross(s, e1);
    float v = dot(dir, q) * inv;
    if (v < 0 || u + v > 1) return false;
    float t = dot(e2, q) * inv;
    if (t <= 1e-4) return false;
    tHit = t;
    return true;
}

uint packRgba(float3 c) {
    uint3 u = uint3(saturate(c) * 255.0);
    return u.r | (u.g << 8) | (u.b << 16) | (255u << 24);
}

[numthreads(8, 8, 1)]
void trace(uint3 id : SV_DispatchThreadID) {
    int grid = gridSampTri.x;
    int samp = max(gridSampTri.y, 1);
    int triCount = gridSampTri.z;
    if (id.x >= (uint)grid || id.y >= (uint)grid) return;

    float3 acc = 0;
    for (int s = 0; s < samp; ++s) {
        float jitter = samp == 1 ? 0.0 : (float(s) / float(samp) - 0.5);
        float hOff = hvFov.z * 0.5 + (float(id.x) + jitter) * (-hvFov.z / float(grid));
        float vOff = -hvFov.z * 0.5 + (float(id.y) + jitter) * (hvFov.z / float(grid));
        float3 dir = camDir(hvFov.x + hOff, hvFov.y + vOff);
        float best = 1e30;
        int hit = -1;
        float3 hitP = 0;
        float3 orig = camera.xyz;
        for (int i = 0; i < triCount; ++i) {
            float t = 0;
            if (hitTri(orig, dir, tris[i], t) && t < best) {
                best = t;
                hit = i;
                hitP = orig + dir * t;
            }
        }
        if (hit >= 0) {
            Triangle tri = tris[hit];
            float3 n = tri.n.xyz;
            float3 toL = light.xyz - hitP;
            if (dot(n, toL) < 0) n = -n;
            float dist = max(0.15, length(toL));
            float ndotl = max(0.0, dot(normalize(n), normalize(toL)));
            float lambert = cos(ndotl * 3.14159265358979323846 / 180.0);
            float3 c = tri.rgb.xyz * (lambert * (1.0 / (dist / 4.0)));
            c = pow(max(c, 1e-6), 0.454545);
            acc += c;
        }
    }
    acc /= float(samp);
    outRgba[id.y * grid + id.x] = packRgba(acc);
}
