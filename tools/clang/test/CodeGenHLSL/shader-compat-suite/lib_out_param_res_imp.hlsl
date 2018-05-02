// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

// CHECK: load %"class.Buffer<vector<float, 4> >", %"class.Buffer<vector<float, 4> >"* @"\01?buf@@3V?$Buffer@V?$vector@M$03@@@@A"
// CHECK: store %"class.Buffer<vector<float, 4> >"
// Make sure resource return type works.

Buffer<float4> buf;

Buffer<float4> GetBuf() {
  return buf;
}