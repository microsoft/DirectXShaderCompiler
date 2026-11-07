// RUN: not %dxc -T cs_6_0 -E main -fcgl %s -spirv 2>&1 | FileCheck %s

// vk::ext_decorate_id and vk::ext_decorate_string have no OpFunction form and
// must be diagnosed. Both are on one function because translation stops after
// the first function with an error.

// CHECK-DAG: error: vk::ext_decorate_id is not supported on functions
// CHECK-DAG: error: vk::ext_decorate_string is not supported on functions
[[vk::ext_decorate_id(/* UniformId */ 27, 13)]]
[[vk::ext_decorate_string(/* UserTypeGOOGLE */ 5636, "myType")]]
[noinline] uint Decorated(uint x) { return x; }

RWStructuredBuffer<uint> buf;

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  buf[0] = Decorated(tid.x);
}
