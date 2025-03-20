// RUN: %dxilver 1.9 | %dxc -T lib_6_9 -validator-version 1.9 %s | FileCheck -check-prefix=NOD3D %s
// RUN: %dxilver 1.9 | %dxc -T lib_6_9 -validator-version 1.9 %s | %D3DReflect %s | FileCheck -check-prefix=D3D %s

// NOD3D: ; RaytracingPipelineConfig1 rpc = { MaxTraceRecursionDepth = 32, Flags = RAYTRACING_PIPELINE_FLAG_SKIP_TRIANGLES | RAYTRACING_PIPELINE_FLAG_ALLOW_OPACITY_MICROMAPS };
// D3D: MinShaderTarget: 0x10069
RaytracingPipelineConfig1 rpc = { 32, RAYTRACING_PIPELINE_FLAG_SKIP_TRIANGLES | RAYTRACING_PIPELINE_FLAG_ALLOW_OPACITY_MICROMAPS };

RaytracingAccelerationStructure RTAS;
// DXR entry point to ensure RDAT flags match during validation.
[shader("vertex")]
void main(RayDesc rayDesc : RAYDESC) {
  RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS> rayQuery1;
  rayQuery1.TraceRayInline(RTAS, RAY_FLAG_FORCE_OMM_2_STATE, 2, rayDesc);
}
