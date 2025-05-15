// RUN: %dxc -Tlib_6_3 -HV 2021 -verify %s
// RUN: %dxc -Tcs_6_0 -HV 2021 -verify %s

ByteAddressBuffer In;
RWBuffer<float> Out;

[shader("compute")]
[numthreads(1,1,1)]
void main()
{ 
  RWBuffer<float> FB = In.Load<RWBuffer<float> >(0);
  // expected-error@-1{{Explicit template arguments on intrinsic Load must be a single numeric type}}
  // expected-error@-2{{object 'RWBuffer<float>' is not allowed in builtin template parameters}}
  Out[0] = FB[0];
}
