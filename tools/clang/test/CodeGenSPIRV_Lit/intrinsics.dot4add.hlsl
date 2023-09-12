// RUN: not %dxc -E frag -T ps_6_4 -fspv-target-env=vulkan1.1 -fcgl  %s -spirv  2>&1 | FileCheck %s

uint frag(float4 vertex
          : SV_POSITION) : SV_Target {
  uint acc = 0;

  // CHECK: 8:14: error: dot4add_u8packed intrinsic function unimplemented
  acc += 1 + dot4add_u8packed(vertex.x, vertex.y, uint(vertex.y));

  // CHECK: 11:14: error: dot4add_i8packed intrinsic function unimplemented
  acc += 2 + dot4add_i8packed(vertex.z, vertex.w, int(vertex.x));

  // CHECK: 14:10: error: dot4add_u8packed intrinsic function unimplemented
  acc += dot4add_u8packed(vertex.x, vertex.y, uint(vertex.y)) + 1;

  // CHECK: 17:13: error: dot4add_u8packed intrinsic function unimplemented
  acc = 1 + dot4add_u8packed(vertex.x, vertex.y, uint(vertex.y));

  // CHECK: 20:9: error: dot4add_u8packed intrinsic function unimplemented
  acc = dot4add_u8packed(vertex.x, vertex.y, uint(vertex.y)) + 1;

  return acc;
}
