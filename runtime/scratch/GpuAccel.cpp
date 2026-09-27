#include "scratch/GpuAccel.hpp"
#include "scratch/GpuBackends.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "scratch/List.hpp"
#include "scratch/Pen.hpp"
#include "scratch/Runtime.hpp"
#include "scratch/Stage.hpp"
#include "scratch/Value.hpp"

#if defined(__APPLE__) && defined(__OBJC__)
#import <Metal/Metal.h>
#endif

namespace scratch {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
};

float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float length(Vec3 v) { return std::sqrt(std::max(0.0f, dot(v, v))); }
Vec3 normalize(Vec3 v) {
    const float len = length(v);
    return len > 1e-8f ? v * (1.0f / len) : Vec3{};
}

struct Triangle {
    Vec3 v0, v1, v2, n, rgb;
};

double listNum(const List* list, size_t oneBased) {
    if (!list || oneBased == 0 || oneBased > list->length()) return 0;
    return list->items()[oneBased - 1].toNumber();
}

const List* stageList(Stage& stage, const char* name) { return stage.listByName(name); }

bool gatherMesh(Stage& stage, std::vector<Triangle>& tris, Vec3& camera, float& camH, float& camV, Vec3& light) {
    const List* xs = stageList(stage, "💾 x ▲");
    const List* ys = stageList(stage, "💾 y ▲");
    const List* zs = stageList(stage, "💾 z ▲");
    if (!xs || !ys || !zs || xs->length() < 3) return false;
    const List* xform = stageList(stage, "🧊Transform (pos)");
    const List* objData = stageList(stage, "🧊object data (data pos)");
    const List* cr = stageList(stage, "💾🟥 face Colour");
    const List* cg = stageList(stage, "💾🟩 face Colour");
    const List* cb = stageList(stage, "💾🟦 face Colour");
    const List* cam = stageList(stage, "📷🌍Render Camera");
    const List* lx = stageList(stage, "💡x ▲");
    const List* ly = stageList(stage, "💡y ▲");
    const List* lz = stageList(stage, "💡z ▲");

    camera = {0, 0, 8};
    camH = 0;
    camV = 0;
    if (cam && cam->length() >= 5) {
        camera = {static_cast<float>(listNum(cam, 1)), static_cast<float>(listNum(cam, 2)),
                  static_cast<float>(listNum(cam, 3))};
        camH = static_cast<float>(listNum(cam, 4));
        camV = static_cast<float>(listNum(cam, 5));
    }
    light = {0, 6, 4};
    if (lx && ly && lz && lx->length() >= 1) {
        light = {static_cast<float>(listNum(lx, 1)), static_cast<float>(listNum(ly, 1)),
                 static_cast<float>(listNum(lz, 1))};
    }

    const size_t objects = objData && objData->length() >= 3 ? objData->length() / 3 : 1;
    size_t next = 1;
    tris.clear();
    for (size_t obj = 1; obj <= objects; ++obj) {
        const size_t faces = objData ? static_cast<size_t>(std::max(0.0, listNum(objData, 3 * obj - 1)))
                                     : xs->length() / 3;
        const float tx = xform ? static_cast<float>(listNum(xform, 3 * obj - 2)) : 0;
        const float ty = xform ? static_cast<float>(listNum(xform, 3 * obj - 1)) : 0;
        const float tz = xform ? static_cast<float>(listNum(xform, 3 * obj)) : 0;
        for (size_t f = 0; f < faces && next + 2 <= xs->length(); ++f, next += 3) {
            Triangle t;
            t.v0 = {static_cast<float>(listNum(xs, next)) + tx, static_cast<float>(listNum(ys, next)) + ty,
                    static_cast<float>(listNum(zs, next)) + tz};
            t.v1 = {static_cast<float>(listNum(xs, next + 1)) + tx, static_cast<float>(listNum(ys, next + 1)) + ty,
                    static_cast<float>(listNum(zs, next + 1)) + tz};
            t.v2 = {static_cast<float>(listNum(xs, next + 2)) + tx, static_cast<float>(listNum(ys, next + 2)) + ty,
                    static_cast<float>(listNum(zs, next + 2)) + tz};
            t.n = normalize(cross(t.v1 - t.v0, t.v2 - t.v0));
            t.rgb = {static_cast<float>(cr ? listNum(cr, next) : 0.8),
                     static_cast<float>(cg ? listNum(cg, next) : 0.8),
                     static_cast<float>(cb ? listNum(cb, next) : 0.8)};
            tris.push_back(t);
        }
    }
    return !tris.empty();
}

bool intersectTri(Vec3 orig, Vec3 dir, const Triangle& tri, float& tHit) {
    const Vec3 e1 = tri.v1 - tri.v0;
    const Vec3 e2 = tri.v2 - tri.v0;
    const Vec3 p = cross(dir, e2);
    const float det = dot(e1, p);
    if (std::fabs(det) < 1e-8f) return false;
    const float inv = 1.0f / det;
    const Vec3 s = orig - tri.v0;
    const float u = dot(s, p) * inv;
    if (u < 0 || u > 1) return false;
    const Vec3 q = cross(s, e1);
    const float v = dot(dir, q) * inv;
    if (v < 0 || u + v > 1) return false;
    const float t = dot(e2, q) * inv;
    if (t <= 1e-4f) return false;
    tHit = t;
    return true;
}

Vec3 shade(const Triangle& tri, Vec3 hit, Vec3 light) {
    Vec3 n = tri.n;
    const Vec3 toL = light - hit;
    if (dot(n, toL) < 0) n = n * -1.0f;
    const float dist = std::max(0.15f, length(toL));
    const float ndotl = std::max(0.0f, dot(normalize(n), normalize(toL)));
    // Scratch applies cos() to the raw dot in degrees, then distance falloff.
    const float lambert = static_cast<float>(std::cos(static_cast<double>(ndotl) * kDeg));
    const float atten = 1.0f / (dist / 4.0f);
    Vec3 c = tri.rgb * (lambert * atten);
    auto gamma = [](float x) {
        x = std::max(x, 1e-6f);
        return static_cast<float>(std::pow(static_cast<double>(x), 0.454545));
    };
    return {gamma(c.x), gamma(c.y), gamma(c.z)};
}

Vec3 traceRay(Vec3 orig, Vec3 dir, const std::vector<Triangle>& tris, Vec3 light) {
    float best = 1e30f;
    const Triangle* hitTri = nullptr;
    Vec3 hitP{};
    for (const Triangle& tri : tris) {
        float t = 0;
        if (intersectTri(orig, dir, tri, t) && t < best) {
            best = t;
            hitTri = &tri;
            hitP = orig + dir * t;
        }
    }
    if (!hitTri) return {};
    return shade(*hitTri, hitP, light);
}

Vec3 cameraDir(float hDeg, float vDeg) {
    const float h = hDeg * static_cast<float>(kDeg);
    const float v = vDeg * static_cast<float>(kDeg);
    return normalize({std::cos(v) * std::sin(h), std::sin(v), std::cos(v) * std::cos(h)});
}

void traceGridCpu(const std::vector<Triangle>& tris, Vec3 camera, float camH, float camV, Vec3 light,
                  int grid, float fov, int samples, std::vector<std::uint8_t>& outRgba) {
    outRgba.assign(static_cast<size_t>(grid) * static_cast<size_t>(grid) * 4, 0);
    const int samp = std::max(1, samples);
    const unsigned workers = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> pool;
    pool.reserve(workers);
    const int rowsPer = (grid + static_cast<int>(workers) - 1) / static_cast<int>(workers);
    for (unsigned w = 0; w < workers; ++w) {
        const int r0 = static_cast<int>(w) * rowsPer;
        const int r1 = std::min(grid, r0 + rowsPer);
        if (r0 >= r1) break;
        pool.emplace_back([&, r0, r1]() {
            for (int row = r0; row < r1; ++row) {
                for (int col = 0; col < grid; ++col) {
                    Vec3 acc{};
                    for (int s = 0; s < samp; ++s) {
                        const float jitter = samp == 1 ? 0.0f : (static_cast<float>(s) / static_cast<float>(samp) - 0.5f);
                        const float hOff = fov * 0.5f + (static_cast<float>(col) + jitter) * (-fov / static_cast<float>(grid));
                        const float vOff = -fov * 0.5f + (static_cast<float>(row) + jitter) * (fov / static_cast<float>(grid));
                        acc = acc + traceRay(camera, cameraDir(camH + hOff, camV + vOff), tris, light);
                    }
                    acc = acc * (1.0f / static_cast<float>(samp));
                    const size_t i = (static_cast<size_t>(row) * static_cast<size_t>(grid) + static_cast<size_t>(col)) * 4;
                    outRgba[i] = static_cast<std::uint8_t>(std::clamp(acc.x, 0.0f, 1.0f) * 255.0f);
                    outRgba[i + 1] = static_cast<std::uint8_t>(std::clamp(acc.y, 0.0f, 1.0f) * 255.0f);
                    outRgba[i + 2] = static_cast<std::uint8_t>(std::clamp(acc.z, 0.0f, 1.0f) * 255.0f);
                    outRgba[i + 3] = 255;
                }
            }
        });
    }
    for (auto& t : pool) t.join();
}

#if defined(__APPLE__) && defined(__OBJC__)

const char* kMetalSrc = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct Triangle {
    float4 v0, v1, v2, n, rgb;
};
struct Params {
    float4 camera;
    float4 hvFov;
    float4 light;
    int grid, samples, triCount, pad;
};

float3 camDir(float hDeg, float vDeg) {
    const float d = 3.14159265358979323846 / 180.0;
    float h = hDeg * d, v = vDeg * d;
    float3 dir = float3(cos(v) * sin(h), sin(v), cos(v) * cos(h));
    float len = length(dir);
    return len > 1e-8 ? dir / len : dir;
}

bool hitTri(float3 orig, float3 dir, Triangle tri, thread float& tHit) {
    float3 e1 = tri.v1.xyz - tri.v0.xyz, e2 = tri.v2.xyz - tri.v0.xyz;
    float3 p = cross(dir, e2);
    float det = dot(e1, p);
    if (fabs(det) < 1e-8) return false;
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

kernel void trace(device const Triangle* tris [[buffer(0)]],
                  constant Params& p [[buffer(1)]],
                  device uchar4* out [[buffer(2)]],
                  uint2 gid [[thread_position_in_grid]]) {
    if (gid.x >= (uint)p.grid || gid.y >= (uint)p.grid) return;
    float3 acc = float3(0);
    int samp = max(p.samples, 1);
    for (int s = 0; s < samp; ++s) {
        float jitter = samp == 1 ? 0.0 : (float(s) / float(samp) - 0.5);
        float hOff = p.hvFov.z * 0.5 + (float(gid.x) + jitter) * (-p.hvFov.z / float(p.grid));
        float vOff = -p.hvFov.z * 0.5 + (float(gid.y) + jitter) * (p.hvFov.z / float(p.grid));
        float3 dir = camDir(p.hvFov.x + hOff, p.hvFov.y + vOff);
        float best = 1e30;
        int hit = -1;
        float3 hitP = float3(0);
        float3 orig = p.camera.xyz;
        for (int i = 0; i < p.triCount; ++i) {
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
            float3 toL = p.light.xyz - hitP;
            if (dot(n, toL) < 0) n = -n;
            float dist = max(0.15, length(toL));
            float ndotl = max(0.0, dot(normalize(n), normalize(toL)));
            float lambert = cos(ndotl * 3.14159265358979323846 / 180.0);
            float3 c = tri.rgb.xyz * (lambert * (1.0 / (dist / 4.0)));
            c = pow(max(c, float3(1e-6)), 0.454545);
            acc += c;
        }
    }
    acc /= float(samp);
    uint idx = gid.y * (uint)p.grid + gid.x;
    out[idx] = uchar4(uchar(clamp(acc.x, 0.0, 1.0) * 255.0),
                      uchar(clamp(acc.y, 0.0, 1.0) * 255.0),
                      uchar(clamp(acc.z, 0.0, 1.0) * 255.0), 255);
}
)METAL";

struct PackedTri {
    float v0[4], v1[4], v2[4], n[4], rgb[4];
};
struct PackedParams {
    float camera[4];
    float camH, camV, fov, pad0;
    float light[4];
    int grid, samples, triCount, pad1;
};

bool traceGridMetal(const std::vector<Triangle>& tris, Vec3 camera, float camH, float camV, Vec3 light,
                    int grid, float fov, int samples, std::vector<std::uint8_t>& outRgba) {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) return false;
    NSError* err = nil;
    MTLCompileOptions* opts = [MTLCompileOptions new];
    id<MTLLibrary> lib = [device newLibraryWithSource:@(kMetalSrc) options:opts error:&err];
    if (!lib) {
        SDL_Log("GPU: Metal compile failed: %s", err ? [[err localizedDescription] UTF8String] : "?");
        return false;
    }
    id<MTLFunction> fn = [lib newFunctionWithName:@"trace"];
    if (!fn) return false;
    id<MTLComputePipelineState> pso = [device newComputePipelineStateWithFunction:fn error:&err];
    if (!pso) return false;
    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (!queue) return false;

    std::vector<PackedTri> packed(tris.size());
    for (size_t i = 0; i < tris.size(); ++i) {
        const Triangle& t = tris[i];
        packed[i] = {{t.v0.x, t.v0.y, t.v0.z, 0},
                     {t.v1.x, t.v1.y, t.v1.z, 0},
                     {t.v2.x, t.v2.y, t.v2.z, 0},
                     {t.n.x, t.n.y, t.n.z, 0},
                     {t.rgb.x, t.rgb.y, t.rgb.z, 0}};
    }
    PackedParams params{};
    params.camera[0] = camera.x;
    params.camera[1] = camera.y;
    params.camera[2] = camera.z;
    params.camH = camH;
    params.camV = camV;
    params.fov = fov;
    params.pad0 = 0;
    params.light[0] = light.x;
    params.light[1] = light.y;
    params.light[2] = light.z;
    params.grid = grid;
    params.samples = std::max(1, samples);
    params.triCount = static_cast<int>(tris.size());

    const NSUInteger triBytes = packed.size() * sizeof(PackedTri);
    const NSUInteger outBytes = static_cast<NSUInteger>(grid) * static_cast<NSUInteger>(grid) * 4;
    id<MTLBuffer> triBuf = [device newBufferWithBytes:packed.data() length:triBytes options:MTLResourceStorageModeShared];
    id<MTLBuffer> parBuf = [device newBufferWithBytes:&params length:sizeof(params) options:MTLResourceStorageModeShared];
    id<MTLBuffer> outBuf = [device newBufferWithLength:outBytes options:MTLResourceStorageModeShared];
    if (!triBuf || !parBuf || !outBuf) return false;

    id<MTLCommandBuffer> cmd = [queue commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
    [enc setComputePipelineState:pso];
    [enc setBuffer:triBuf offset:0 atIndex:0];
    [enc setBuffer:parBuf offset:0 atIndex:1];
    [enc setBuffer:outBuf offset:0 atIndex:2];
    const MTLSize threads = MTLSizeMake(8, 8, 1);
    const MTLSize groups = MTLSizeMake((grid + 7) / 8, (grid + 7) / 8, 1);
    [enc dispatchThreadgroups:groups threadsPerThreadgroup:threads];
    [enc endEncoding];
    [cmd commit];
    [cmd waitUntilCompleted];

    outRgba.resize(outBytes);
    std::memcpy(outRgba.data(), [outBuf contents], outBytes);
    return true;
}

#endif  // Apple Metal

void packGpuMesh(const std::vector<Triangle>& tris, Vec3 camera, float camH, float camV, Vec3 light, int grid,
                 float fov, int samples, std::vector<GpuTriangle>& packed, GpuParams& params) {
    packed.resize(tris.size());
    for (size_t i = 0; i < tris.size(); ++i) {
        const Triangle& t = tris[i];
        packed[i] = {{t.v0.x, t.v0.y, t.v0.z, 0},
                     {t.v1.x, t.v1.y, t.v1.z, 0},
                     {t.v2.x, t.v2.y, t.v2.z, 0},
                     {t.n.x, t.n.y, t.n.z, 0},
                     {t.rgb.x, t.rgb.y, t.rgb.z, 0}};
    }
    params = {};
    params.camera[0] = camera.x;
    params.camera[1] = camera.y;
    params.camera[2] = camera.z;
    params.camH = camH;
    params.camV = camV;
    params.fov = fov;
    params.light[0] = light.x;
    params.light[1] = light.y;
    params.light[2] = light.z;
    params.grid = grid;
    params.samples = std::max(1, samples);
    params.triCount = static_cast<int>(tris.size());
}

void blitGridToPen(PenLayer& pen, const std::vector<std::uint8_t>& rgba, int grid, double res) {
    const double x0 = -240.0;
    const double y0 = 122.0;
    for (int row = 0; row < grid; ++row) {
        for (int col = 0; col < grid; ++col) {
            const size_t i = (static_cast<size_t>(row) * static_cast<size_t>(grid) + static_cast<size_t>(col)) * 4;
            pen.fillStageBox(x0 + col * res, y0 - row * res, x0 + (col + 1) * res, y0 - (row + 1) * res, rgba[i],
                             rgba[i + 1], rgba[i + 2], rgba[i + 3]);
        }
    }
}

}  // namespace

bool tryGpuTraceToPen(Runtime& runtime, double resolution, double samples, double fovDegrees) {
    if (!runtime.config().gpuAccel) return false;
    const double res = std::max(1.0, resolution);
    const int grid = std::max(1, static_cast<int>(std::floor(300.0 / res)));
    std::vector<Triangle> tris;
    Vec3 camera, light;
    float camH = 0, camV = 0;
    if (!gatherMesh(runtime.stage(), tris, camera, camH, camV, light)) return false;

    std::vector<std::uint8_t> rgba;
    const float fov = static_cast<float>(fovDegrees);
    const int samp = static_cast<int>(std::lround(samples));
    bool usedGpu = false;
#if defined(__APPLE__) && defined(__OBJC__)
    usedGpu = traceGridMetal(tris, camera, camH, camV, light, grid, fov, samp, rgba);
    if (usedGpu) SDL_Log("GPU: Metal path traced %d triangles at %dx%d", static_cast<int>(tris.size()), grid, grid);
#endif
    if (!usedGpu) {
        std::vector<GpuTriangle> packed;
        GpuParams params{};
        packGpuMesh(tris, camera, camH, camV, light, grid, fov, samp, packed, params);
        usedGpu = gpuTraceVulkan(packed, params, rgba);
        if (!usedGpu) usedGpu = gpuTraceD3D11(packed, params, rgba);
        if (usedGpu) {
            SDL_Log("GPU: API path traced %d triangles at %dx%d", static_cast<int>(tris.size()), grid, grid);
        }
    }
    if (!usedGpu) {
        SDL_Log("GPU: threaded CPU path tracing %d triangles at %dx%d", static_cast<int>(tris.size()), grid, grid);
        traceGridCpu(tris, camera, camH, camV, light, grid, fov, samp, rgba);
    }
    runtime.penLayer().clear();
    blitGridToPen(runtime.penLayer(), rgba, grid, res);
    runtime.requestRedraw();
    return true;
}

}  // namespace scratch
