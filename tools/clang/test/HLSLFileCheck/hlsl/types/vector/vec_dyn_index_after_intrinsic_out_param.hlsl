// REQUIRES: dxil-1-10
// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s --check-prefix=OPT
// RUN: %dxc -E main -T ps_6_0 -Od %s | FileCheck %s --check-prefix=NOOPT
// RUN: %dxc -E main2 -T cs_6_10 -HV 2021 %s -Fo %t
// RUN: %dxc -E main2 -T cs_6_10 -HV 2021 -Od %s -Fo %t

// Regression test for GitHub #9050: dynamically indexing a one-element
// vector that an intrinsic wrote through its out parameter.
//
// The unresolved dx.hl.op intrinsic call keeps the vector alloca as its
// out pointer argument. DynamicIndexingVectorToArray rewrites loads,
// stores, bitcasts and geps of the alloca into the lowered array, but
// had no way to rewrite the call user and hit "not implement yet": in
// release builds the assert is compiled out and the alloca was erased
// while the call still used it, corrupting the module. The call is now
// redirected to a temporary vector alloca whose value is copied into
// the array right after the call.

#include <dx/linalg.h>

// The integer part of modf(1.5, ...) is 1.0, and the dynamic index reads
// the only element of the one-element vector.
// OPT: storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 1.000000e+00)

// NOOPT: alloca {{\[1 x float\]}}
// NOOPT: getelementptr {{\[1 x float\]}}{{.*}}i32 %
// NOOPT: storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %

// The DXIL must not contain any vector types.
// OPT-NOT: <1 x
// NOOPT-NOT: <1 x

float main(uint I : INDEX) : SV_Target {
  float1 Integral;
  modf((float1)1.5, Integral);
  return Integral[I];
}

// The original reproducer from the issue: a LinAlg Convert writing into
// a vector<uint, 1> that is then dynamically indexed.
RWByteAddressBuffer Output : register(u0);
[numthreads(1, 1, 1)]
void main2() {
  vector<float, 4> InVec = {1.0, 2.0, 3.0, 4.0};
  vector<uint, 1> OutVec;
  dx::__builtin_LinAlg_Convert(OutVec, InVec, 9, 19);
  for (uint I = 0; I < 1; ++I)
    Output.Store<uint>(I * 4, OutVec[I]);
}
