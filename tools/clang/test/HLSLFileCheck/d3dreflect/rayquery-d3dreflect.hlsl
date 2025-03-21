// RUN: %dxilver 1.9 | %dxc -T lib_6_9 -validator-version 1.9 %s | %D3DReflect %s | FileCheck %s

// CHECK: MinShaderTarget: 0x10069
RaytracingPipelineConfig1 rpc = { 32, RAYTRACING_PIPELINE_FLAG_SKIP_TRIANGLES | RAYTRACING_PIPELINE_FLAG_ALLOW_OPACITY_MICROMAPS };
