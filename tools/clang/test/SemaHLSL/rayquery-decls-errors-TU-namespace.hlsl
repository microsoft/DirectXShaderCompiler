// RUN: %dxc -T vs_6_5 -E main -verify %s

// tests that RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS usage will emit
// errors when the flag appears in a namespace, and when compilation target
// is less than shader model 6.9.

// Note, the first warning is generated as a result of usage within the namespace itself, and
// the second warning is generated as a result of reference to the namespace variable 
// when defining the int x variable.

namespace MyNamespace {
  // expected-warning@+2{{potential misuse of built-in constant RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS in shader model vs_6_5; introduced in shader model 6.9}} 
  // expected-warning@+1{{potential misuse of built-in constant RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS in shader model vs_6_5; introduced in shader model 6.9}}
  static const int badVar = RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS;
}

RaytracingAccelerationStructure RTAS;
void main(uint i : IDX, RayDesc rayDesc : RAYDESC) {

  int x = MyNamespace::badVar;
  RayQuery<0> rayQuery0a;

  if (x > 4){
    rayQuery0a.TraceRayInline(RTAS, 8, 2, rayDesc);
  }
  else{
    rayQuery0a.TraceRayInline(RTAS, 16, 2, rayDesc);
  }
}
