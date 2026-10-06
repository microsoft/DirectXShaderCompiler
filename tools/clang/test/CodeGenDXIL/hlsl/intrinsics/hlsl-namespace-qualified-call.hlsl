// RUN: %dxc -T cs_6_6 -HV 202x -E main %s | FileCheck %s

// Verify that under HLSL 202x an HLSL intrinsic invoked through an
// explicit 'hlsl::' qualifier compiles and lowers to the expected DXIL
// operation, even when there is no prior unqualified use to materialize
// the declaration in the namespace.

RWBuffer<float> Out;

[numthreads(1,1,1)]
void main(uint tid : SV_DispatchThreadID) {
  float x = (float)tid;
  float y = (float)(tid + 1);
  // Both calls go through the implicit 'hlsl' namespace.
  float a = hlsl::sin(x);
  float b = hlsl::sin(y);
  Out[tid] = a + b;
}

// Both calls should lower to dx.op.unary Sin (opcode 13).
// CHECK: call float @dx.op.unary.f32(i32 13, float
// CHECK: call float @dx.op.unary.f32(i32 13, float
