// RUN: %dxc -T vs_6_5 -E main -verify %s

// tests that RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS usage will emit
// exactly one warning when multiple incompatible 
// availability attribute decls appear, and the compilation target
// is less than shader model 6.9.

namespace MyNamespace {
  // expected-warning@+1{{potential misuse of built-in constant 'RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS' in shader model vs_6_5; introduced in shader model 6.9}}
  static const int badVar = RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS;
}

groupshared const int otherBadVar = RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS;

RaytracingAccelerationStructure RTAS;
void main(uint i : IDX, RayDesc rayDesc : RAYDESC) {

  int x = MyNamespace::badVar + otherBadVar;
  RayQuery<0> rayQuery0a;

  if (x > 4){
    rayQuery0a.TraceRayInline(RTAS, 8, 2, rayDesc);
  }
  else{
    rayQuery0a.TraceRayInline(RTAS, 16, 2, rayDesc);
  }
}
