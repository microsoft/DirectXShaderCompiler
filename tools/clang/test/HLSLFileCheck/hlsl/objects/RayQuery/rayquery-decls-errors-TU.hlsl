// RUN: %dxc -T vs_6_5 -E main %s | FileCheck %s -check-prefix=UNDER69

void main(uint i : IDX, RayDesc rayDesc : RAYDESC) {

  // UNDER69: error: A non-zero value for the RayQueryFlags template argument requires shader model 6.9 or above.
  RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, 1> rayQuery0b;

  // UNDER69: warning: potential misuse of built-in constant RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS in shader model vs_6_5; introduced in shader model 6.9
  RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS> rayQuery0d;

}
