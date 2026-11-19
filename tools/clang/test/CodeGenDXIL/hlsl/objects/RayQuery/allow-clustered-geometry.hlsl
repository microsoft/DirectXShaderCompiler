// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 %s | FileCheck %s
// RUN: %dxc -T lib_6_10 %s | FileCheck %s
// RUN: %dxc -T cs_6_10 -fcgl %s | FileCheck -check-prefix=FCGL %s
// RUN: not %dxc -T cs_6_9 -WX %s 2>&1 | FileCheck -check-prefix=SM69 %s

// SM69: error: potential misuse of built-in constant 'RAYQUERY_FLAG_ALLOW_CLUSTERED_GEOMETRY' in shader model cs_6_9; introduced in shader model 6.10

RaytracingAccelerationStructure RTAS;
RWByteAddressBuffer output;

[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
  RayDesc ray = (RayDesc)0;
  ray.TMax = 1;

  // CHECK: call i32 @dx.op.allocateRayQuery2(i32 258, i32 0, i32 2)
  // FCGL: call i32 @"dx.hl.op..i32 (i32, i32, i32)"(i32 4, i32 0, i32 2)
  RayQuery<RAY_FLAG_NONE, RAYQUERY_FLAG_ALLOW_CLUSTERED_GEOMETRY> clustered;
  clustered.TraceRayInline(RTAS, RAY_FLAG_NONE, 0xff, ray);
  output.Store(0, clustered.CommittedStatus());

  // CHECK: call i32 @dx.op.allocateRayQuery2(i32 258, i32 1024, i32 3)
  // FCGL: call i32 @"dx.hl.op..i32 (i32, i32, i32)"(i32 4, i32 1024, i32 3)
  RayQuery<RAY_FLAG_FORCE_OMM_2_STATE,
           RAYQUERY_FLAG_ALLOW_CLUSTERED_GEOMETRY |
               RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS> combined;
  combined.TraceRayInline(RTAS, RAY_FLAG_NONE, 0xff, ray);
  output.Store(4, combined.CommittedStatus());
}
