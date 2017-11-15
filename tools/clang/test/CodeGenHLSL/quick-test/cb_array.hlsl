// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// Make sure no lshr created for cbuffer array.
// CHECK-NOT: lshr

float A[6] : register(b0);
float main(int i : A) : SV_TARGET
{
  return A[i] + A[i+1] + A[i+2] ;
}
