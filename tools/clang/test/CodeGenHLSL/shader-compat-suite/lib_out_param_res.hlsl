// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// CHECK: call void @"\01?GetBuf@@YA?AV?$Buffer@V?$vector@M$03@@@@XZ"(%dx.types.Handle* nonnull %{{.*}})
// Make sure resource return type works.

Buffer<float4> GetBuf();

float4 test(uint i) {
  Buffer<float4> buf = GetBuf();
  return buf[i];
}