// RUN: %dxc -T vs_6_5 -E main -verify %s

// Test that at the call site of any TraceRayInline call, a default error
// warning is emitted that indicates the ray query object has the
// RAY_FLAG_FORCE_OMM_2_STATE set, but doesn't have 
// RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS set

RaytracingAccelerationStructure RTAS;
float main(RayDesc rayDesc : RAYDESC) : OUT {
  // expected-note@+2{{RayQueryFlags declared here}}
  // expected-note@+1{{RayQueryFlags declared here}}
  RayQuery<0> rayQuery; // implicitly, the second arg is 0.

  // expected-error@+1{{When using 'RAY_FLAG_FORCE_OMM_2_STATE' in RayFlags, RayQueryFlags must have RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS set.}}
  rayQuery.TraceRayInline(RTAS, RAY_FLAG_FORCE_OMM_2_STATE, 2, rayDesc);
  
  // expected-error@+1{{When using 'RAY_FLAG_FORCE_OMM_2_STATE' in RayFlags, RayQueryFlags must have RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS set.}}
  rayQuery.TraceRayInline(RTAS, 1024, 2, rayDesc);

  return 0;
}
