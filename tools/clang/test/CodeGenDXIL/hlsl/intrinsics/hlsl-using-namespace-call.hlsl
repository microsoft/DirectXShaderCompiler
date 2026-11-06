// RUN: %dxc -T cs_6_6 -HV 202x -E main %s | FileCheck %s

// Verify that under HLSL 202x an explicit `using namespace hlsl;`
// directive enables unqualified intrinsic calls all the way through
// CodeGen: the resulting DXIL must contain the expected dx.op.unary
// `Sin` operation lowered from the unqualified `sin` call.

using namespace hlsl;

RWBuffer<float> Out;

[numthreads(1,1,1)]
void main(uint tid : SV_DispatchThreadID) {
  float x = (float)tid;
  // Unqualified call resolves through the `using namespace hlsl;`
  // directive above.
  Out[tid] = sin(x);
}

// CHECK: call float @dx.op.unary.f32(i32 13, float
