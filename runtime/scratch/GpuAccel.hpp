// Optional GPU / parallel accelerator for heavy pen-mesh rendering.
// Off by default. Enable with --gpu or SCRATCH_GPU=1.
#pragma once

namespace scratch {

class Runtime;

// When gpuAccel is on, traces the stage mesh (triangle lists + camera) onto
// the pen layer. Returns false if there is no usable mesh so the caller can
// fall back to the original Scratch scripts.
bool tryGpuTraceToPen(Runtime& runtime, double resolution, double samples, double fovDegrees);

}  // namespace scratch
