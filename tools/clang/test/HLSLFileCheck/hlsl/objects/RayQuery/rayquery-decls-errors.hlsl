// RUN: %dxc -T vs_6_5 -E main %s | FileCheck %s -check-prefix=UNDER69
// RUN: %dxc -T vs_6_9 -E RayQueryTests %s | FileCheck %s -check-prefix=ATLEAST69

void main(uint i : IDX, RayDesc rayDesc : RAYDESC) {

  // UNDER69: error: When using 'RAY_FLAG_FORCE_OMM_2_STATE' in RayFlags, RayQueryFlags must have RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS set
  RayQuery<RAY_FLAG_FORCE_OMM_2_STATE> rayQuery0c;  

}

// validate OMM flags
void RayQueryTests(uint i : IDX, RayDesc rayDesc : RAYDESC) {
  // ATLEAST69: error: When using 'RAY_FLAG_FORCE_OMM_2_STATE' in RayFlags, RayQueryFlags must have RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS set.
  RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, 0> rayQuery0a;
}
