#include "scratch/GpuBackends.hpp"

#ifndef _WIN32
namespace scratch {
bool gpuTraceD3D11(const std::vector<GpuTriangle>&, const GpuParams&, std::vector<std::uint8_t>&) {
    return false;
}
}  // namespace scratch
#else

#include <SDL3/SDL.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <cstring>

using Microsoft::WRL::ComPtr;

namespace scratch {
namespace {

const char kHlsl[] = R"HLSL(
struct Triangle { float4 v0; float4 v1; float4 v2; float4 n; float4 rgb; };
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
    float h = hDeg * d, v = vDeg * d;
    float3 dir = float3(cos(v) * sin(h), sin(v), cos(v) * cos(h));
    float len = length(dir);
    return len > 1e-8 ? dir / len : dir;
}
bool hitTri(float3 orig, float3 dir, Triangle tri, out float tHit) {
    float3 e1 = tri.v1.xyz - tri.v0.xyz, e2 = tri.v2.xyz - tri.v0.xyz;
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
    int grid = gridSampTri.x, samp = max(gridSampTri.y, 1), triCount = gridSampTri.z;
    if (id.x >= (uint)grid || id.y >= (uint)grid) return;
    float3 acc = 0;
    for (int s = 0; s < samp; ++s) {
        float jitter = samp == 1 ? 0.0 : (float(s) / float(samp) - 0.5);
        float hOff = hvFov.z * 0.5 + (float(id.x) + jitter) * (-hvFov.z / float(grid));
        float vOff = -hvFov.z * 0.5 + (float(id.y) + jitter) * (hvFov.z / float(grid));
        float3 dir = camDir(hvFov.x + hOff, hvFov.y + vOff);
        float best = 1e30; int hit = -1; float3 hitP = 0; float3 orig = camera.xyz;
        for (int i = 0; i < triCount; ++i) {
            float t = 0;
            if (hitTri(orig, dir, tris[i], t) && t < best) { best = t; hit = i; hitP = orig + dir * t; }
        }
        if (hit >= 0) {
            Triangle tri = tris[hit];
            float3 n = tri.n.xyz; float3 toL = light.xyz - hitP;
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
)HLSL";

}  // namespace

bool gpuTraceD3D11(const std::vector<GpuTriangle>& tris, const GpuParams& params,
                   std::vector<std::uint8_t>& outRgba) {
    if (tris.empty() || params.grid <= 0) return false;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> ctx;
    D3D_FEATURE_LEVEL fl{};
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                 &device, &fl, &ctx))) {
        return false;
    }
    ComPtr<ID3DBlob> cs, err;
    if (FAILED(D3DCompile(kHlsl, sizeof(kHlsl) - 1, "trace.hlsl", nullptr, nullptr, "trace", "cs_5_0", 0, 0, &cs,
                          &err))) {
        if (err) SDL_Log("GPU: D3DCompile failed: %s", static_cast<const char*>(err->GetBufferPointer()));
        return false;
    }
    ComPtr<ID3D11ComputeShader> shader;
    if (FAILED(device->CreateComputeShader(cs->GetBufferPointer(), cs->GetBufferSize(), nullptr, &shader))) {
        return false;
    }

    const UINT triBytes = static_cast<UINT>(tris.size() * sizeof(GpuTriangle));
    const UINT outCount = static_cast<UINT>(params.grid * params.grid);
    const UINT outBytes = outCount * 4;

    D3D11_BUFFER_DESC tbd{};
    tbd.ByteWidth = triBytes;
    tbd.Usage = D3D11_USAGE_DEFAULT;
    tbd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    tbd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    tbd.StructureByteStride = sizeof(GpuTriangle);
    D3D11_SUBRESOURCE_DATA tsd{tris.data(), 0, 0};
    ComPtr<ID3D11Buffer> triBuf;
    if (FAILED(device->CreateBuffer(&tbd, &tsd, &triBuf))) return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
    srvd.Format = DXGI_FORMAT_UNKNOWN;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvd.Buffer.NumElements = static_cast<UINT>(tris.size());
    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(device->CreateShaderResourceView(triBuf.Get(), &srvd, &srv))) return false;

    D3D11_BUFFER_DESC ubd{};
    ubd.ByteWidth = (sizeof(GpuParams) + 15u) & ~15u;
    ubd.Usage = D3D11_USAGE_DYNAMIC;
    ubd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ubd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ComPtr<ID3D11Buffer> ubuf;
    if (FAILED(device->CreateBuffer(&ubd, nullptr, &ubuf))) return false;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(ctx->Map(ubuf.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return false;
    std::memcpy(mapped.pData, &params, sizeof(params));
    ctx->Unmap(ubuf.Get(), 0);

    D3D11_BUFFER_DESC obd{};
    obd.ByteWidth = outBytes;
    obd.Usage = D3D11_USAGE_DEFAULT;
    obd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    obd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    obd.StructureByteStride = 4;
    ComPtr<ID3D11Buffer> outBuf;
    if (FAILED(device->CreateBuffer(&obd, nullptr, &outBuf))) return false;
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavd{};
    uavd.Format = DXGI_FORMAT_UNKNOWN;
    uavd.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavd.Buffer.NumElements = outCount;
    ComPtr<ID3D11UnorderedAccessView> uav;
    if (FAILED(device->CreateUnorderedAccessView(outBuf.Get(), &uavd, &uav))) return false;

    D3D11_BUFFER_DESC sbd = obd;
    sbd.Usage = D3D11_USAGE_STAGING;
    sbd.BindFlags = 0;
    sbd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    sbd.MiscFlags = 0;
    sbd.StructureByteStride = 0;
    ComPtr<ID3D11Buffer> staging;
    if (FAILED(device->CreateBuffer(&sbd, nullptr, &staging))) return false;

    ctx->CSSetShader(shader.Get(), nullptr, 0);
    ctx->CSSetShaderResources(0, 1, srv.GetAddressOf());
    ctx->CSSetConstantBuffers(0, 1, ubuf.GetAddressOf());
    ctx->CSSetUnorderedAccessViews(0, 1, uav.GetAddressOf(), nullptr);
    ctx->Dispatch(static_cast<UINT>((params.grid + 7) / 8), static_cast<UINT>((params.grid + 7) / 8), 1);
    ctx->CopyResource(staging.Get(), outBuf.Get());
    if (FAILED(ctx->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return false;
    outRgba.resize(outBytes);
    std::memcpy(outRgba.data(), mapped.pData, outBytes);
    ctx->Unmap(staging.Get(), 0);
    SDL_Log("GPU: Direct3D 11 compute path");
    return true;
}

}  // namespace scratch

#endif
