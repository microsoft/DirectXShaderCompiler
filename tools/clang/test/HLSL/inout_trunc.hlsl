// RUN: %clang_cc1 -fsyntax-only -Wno-unused-value -ffreestanding -verify %s

// Test that no crashes result when a scalar is provided to an outvar
// and that the new warning is produced.

// CHK_NOCRASH: NocrashMain

float val1;
float val2;
float val3;

float2 vec2;
float3 vec3;
float4 vec4;

void TakeItOut(out float2 it) {
  it = val1;
}

void TakeItIn(inout float3 it) {
  it = val2;
}

void TakeItIn2(inout float4 it) {
  it += val3;
}

void TakeEmOut(out float2 em) {
  em = vec2;
}

void TakeEmIn(inout float3 em) {
  em = vec3;
}

void TakeEmIn2(inout float4 em) {
  em += vec4;
}


float2 RunTest(float it, float em)
{
  float c = it;
  TakeItOut(it); // expected-warning{{implicit truncation of vector type}}
  TakeItIn(it); // expected-warning{{implicit truncation of vector type}}
  TakeItIn2(it); // expected-warning{{implicit truncation of vector type}}

  TakeEmOut(em); // expected-warning{{implicit truncation of vector type}}
  TakeEmIn(em); // expected-warning{{implicit truncation of vector type}} 
  TakeEmIn2(em); // expected-warning{{implicit truncation of vector type}}
  return float2(it, em);
}

float2 main(float it: A, float em: B) : SV_Target
{
  return RunTest(it, em);
}

