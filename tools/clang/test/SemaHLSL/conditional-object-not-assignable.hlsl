// RUN: %dxc -Tcs_6_0 -verify %s

// Like for scalars, vectors and matrices, a conditional operator on objects
// yields an rvalue, so it can't be assigned to.

RWByteAddressBuffer gBuf0 : register(u0);
RWByteAddressBuffer gBuf1 : register(u1);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  RWByteAddressBuffer a = gBuf0;
  RWByteAddressBuffer b = gBuf1;
  (true ? a : b) = gBuf1; /* expected-error {{expression is not assignable}} */

  uint x = 1, y = 2;
  (true ? x : y) = 5; /* expected-error {{expression is not assignable}} */

  // Using the result as an rvalue is still fine.
  RWByteAddressBuffer c = true ? a : b;
  c.Store(0, 1);
  (false ? a : b).Store(4, 2);
}
