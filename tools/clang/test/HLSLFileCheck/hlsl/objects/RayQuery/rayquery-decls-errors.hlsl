// RUN: %dxc -T vs_6_5 -E main %s | FileCheck %s -check-prefix=UNDER69
// RUN: %dxc -T vs_6_9 -E RayQueryTests %s | FileCheck %s -check-prefix=ATLEAST69

int retNum(){
  // expected-warning@+1{{potential misuse of built-in constant RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS in shader model vs_6_5; introduced in shader model 6.9}}
  return RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS;
}

void main(uint i : IDX, RayDesc rayDesc : RAYDESC) {

  int n = retNum();
  RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, n> rayQuery0a;

  // UNDER69: error: A non-zero value for the RayQueryFlags template argument requires shader model 6.9 or above.
  RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, 1> rayQuery0b;

  // UNDER69: error: When using 'RAY_FLAG_FORCE_OMM_2_STATE' in RayFlags, RayQueryFlags must have RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS set
  RayQuery<RAY_FLAG_FORCE_OMM_2_STATE> rayQuery0c;
  
  // UNDER69: warning: potential misuse of built-in constant RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS in shader model vs_6_5; introduced in shader model 6.9
  RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS> rayQuery0d;

}

// validate OMM flags
void RayQueryTests(uint i : IDX, RayDesc rayDesc : RAYDESC) {
  // ATLEAST69: error: When using 'RAY_FLAG_FORCE_OMM_2_STATE' in RayFlags, RayQueryFlags must have RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS set.
  RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, 0> rayQuery0a;
}
