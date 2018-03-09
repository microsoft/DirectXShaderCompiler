// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// CHECK: call void @"\01?GetBuf@@YA?AV?$Buffer@V?$vector@M$03@@@@XZ"(%"class.Buffer<vector<float, 4> >"* nonnull sret {{.*}})
// Make sure resource return type works.

Buffer<float4> GetBuf();

[shader("pixel")]
float4 test(uint i:I) : SV_Target {
  Buffer<float4> buf = GetBuf();
  return buf[i];
}