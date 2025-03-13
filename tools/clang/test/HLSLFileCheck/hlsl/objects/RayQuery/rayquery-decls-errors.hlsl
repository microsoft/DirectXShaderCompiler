// RUN: %dxc -T vs_6_5 -E main -verify %s 

void main(uint i : IDX, RayDesc rayDesc : RAYDESC) {
  // expected-error@+1{{When using 'RAY_FLAG_FORCE_OMM_2_STATE' in RayFlags, RayQueryFlags must have RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS set.}}
  RayQuery<RAY_FLAG_FORCE_OMM_2_STATE> rayQuery0c;  
}
