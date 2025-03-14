// RUN: %dxc -T lib_6_9 %s | FileCheck -check-prefix=ATLEAST69 %s 

// NOD3D: ; RaytracingPipelineConfig1 rpc = { MaxTraceRecursionDepth = 32, Flags = RAYTRACING_PIPELINE_FLAG_SKIP_TRIANGLES | RAYTRACING_PIPELINE_FLAG_ALLOW_OPACITY_MICROMAPS };
// D3D: MinShaderTarget: 0xf0069

RaytracingAccelerationStructure RTAS;
// DXR entry point to ensure RDAT flags match during validation.
[shader("vertex")]
void main(RayDesc rayDesc : RAYDESC) {

  // ATLEAST69: call i32 @dx.op.allocateRayQuery2(i32 258, i32 1024, i32 1)  ; AllocateRayQuery2(constRayFlags,constRayQueryFlags)
  RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS> rayQuery1;

  // ATLEAST69: call i32 @dx.op.allocateRayQuery2(i32 258, i32 1, i32 0)  ; AllocateRayQuery2(constRayFlags,constRayQueryFlags)
  RayQuery<1> rayQuery2;

  rayQuery1.TraceRayInline(RTAS, RAY_FLAG_FORCE_OMM_2_STATE, 2, rayDesc);
}
