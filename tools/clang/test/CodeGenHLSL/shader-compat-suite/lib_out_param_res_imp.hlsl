// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// CHECK: load %class.Buffer, %class.Buffer* @"\01?buf@@3V?$Buffer@V?$vector@M$03@@@@A"
// CHECK: store %class.Buffer
// Make sure resource return type works.

Buffer<float4> buf;

Buffer<float4> GetBuf() {
  return buf;
}