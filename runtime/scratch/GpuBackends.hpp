#pragma once

#include <cstdint>
#include <vector>

namespace scratch {

struct GpuTriangle {
    float v0[4], v1[4], v2[4], n[4], rgb[4];
};

struct GpuParams {
    float camera[4];
    float camH, camV, fov, pad0;
    float light[4];
    int grid, samples, triCount, pad1;
};

bool gpuTraceVulkan(const std::vector<GpuTriangle>& tris, const GpuParams& params,
                    std::vector<std::uint8_t>& outRgba);
bool gpuTraceD3D11(const std::vector<GpuTriangle>& tris, const GpuParams& params,
                   std::vector<std::uint8_t>& outRgba);

}  // namespace scratch
