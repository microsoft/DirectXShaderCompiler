// RUN: %dxc -T vs_6_5 -E main -verify %s 

int retNum(){
  // expected-warning@+1{{potential misuse of built-in constant RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS in shader model vs_6_5; introduced in shader model 6.9}}
  return RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS;
}

RaytracingAccelerationStructure RTAS;
void main(uint i : IDX, RayDesc rayDesc : RAYDESC) {

  int n = retNum();
  RayQuery<0> rayQuery0a;

  if (n > 4){
    rayQuery0a.TraceRayInline(RTAS, 8, 2, rayDesc);
  }
  else{
    rayQuery0a.TraceRayInline(RTAS, 16, 2, rayDesc);
  }
}
