// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 %s -Fo %t -Fc %t.ll && FileCheck -check-prefixes=CHECK,COMBINED %s --input-file=%t.ll
// RUN: not %dxc -T cs_6_9 -WX %s 2>&1 | FileCheck -check-prefix=SM69 %s
// RUN: %dxa %t -dumprdat | FileCheck -check-prefix=RDAT %s
// RUN: not %dxl -T cs_6_9 -E main %t 2>&1 | FileCheck -check-prefix=LINK69 %s
// RUN: %dxl -T cs_6_10 -E main %t | FileCheck %s
// RUN: %dxl -T cs_6_9 -E opacityMicromaps %t | FileCheck -check-prefix=OMM %s

// SM69: error: potential misuse of built-in constant 'RAYQUERY_FLAG_ALLOW_CLUSTERED_GEOMETRY' in shader model cs_6_9; introduced in shader model 6.10
// LINK69: error: Function uses features incompatible with the shader model.

RaytracingAccelerationStructure RTAS;
RWByteAddressBuffer output;

// RDAT-LABEL: UnmangledName: "main"
// RDAT: MinShaderTarget: 0x5006a
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
  RayDesc ray = (RayDesc)0;
  ray.TMax = 1;

  // CHECK: call i32 @dx.op.allocateRayQuery2(i32 258, i32 0, i32 2)
  RayQuery<RAY_FLAG_NONE, RAYQUERY_FLAG_ALLOW_CLUSTERED_GEOMETRY> clustered;
  clustered.TraceRayInline(RTAS, RAY_FLAG_NONE, 0xff, ray);
  output.Store(0, clustered.CommittedStatus());
}

// RDAT-LABEL: UnmangledName: "combined"
// RDAT: MinShaderTarget: 0x5006a
[shader("compute")]
[numthreads(1, 1, 1)]
void combined() {
  RayDesc ray = (RayDesc)0;
  ray.TMax = 1;

  // COMBINED: call i32 @dx.op.allocateRayQuery2(i32 258, i32 1024, i32 3)
  RayQuery<RAY_FLAG_FORCE_OMM_2_STATE,
           RAYQUERY_FLAG_ALLOW_CLUSTERED_GEOMETRY |
               RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS> combined;
  combined.TraceRayInline(RTAS, RAY_FLAG_NONE, 0xff, ray);
  output.Store(4, combined.CommittedStatus());
}

// RDAT-LABEL: UnmangledName: "opacityMicromaps"
// RDAT: MinShaderTarget: 0x50069
[shader("compute")]
[numthreads(1, 1, 1)]
void opacityMicromaps() {
  RayDesc ray = (RayDesc)0;
  ray.TMax = 1;

  // OMM: call i32 @dx.op.allocateRayQuery2(i32 258, i32 1024, i32 1)
  RayQuery<RAY_FLAG_FORCE_OMM_2_STATE,
           RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS> query;
  query.TraceRayInline(RTAS, RAY_FLAG_NONE, 0xff, ray);
  output.Store(8, query.CommittedStatus());
}
