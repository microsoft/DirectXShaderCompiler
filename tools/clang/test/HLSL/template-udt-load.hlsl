// RUN: %clang_cc1 -fsyntax-only -ffreestanding -HV 2021 -verify %s

ByteAddressBuffer In;
RWBuffer<float> Out;


[numthreads(1,1,1)]
void CSMain()
{ 
  RWBuffer<float> FB = In.Load<RWBuffer<float> >(0); // expected-error {{Explicit template arguments on intrinsic Load must be a single numeric type}}
  Out[0] = FB[0];
}
