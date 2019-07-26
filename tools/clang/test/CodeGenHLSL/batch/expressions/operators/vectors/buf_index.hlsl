// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// Tests vector index on buffer load.
// CHECK:alloca [4 x float]
// CHECK:dx.op.bufferLoad.f32
// CHECK:store
// CHECK:store
// CHECK:store
// CHECK:store
// CHECK:load

Buffer<float4> buf;

float4 main(uint i:I) : SV_Target {
  return buf[2][i];
}