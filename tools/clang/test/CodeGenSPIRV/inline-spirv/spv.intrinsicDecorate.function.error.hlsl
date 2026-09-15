// RUN: not %dxc -T cs_6_0 -E main -fcgl %s -spirv 2>&1 | FileCheck %s

// vk::ext_decorate_id and vk::ext_decorate_string decorate a value-id target
// and have no OpFunction-target form, so applying either to a function must be
// diagnosed rather than silently dropped. Both are placed on one function so a
// single compilation reports both (translation stops after the first function
// that errors, so separate functions would only surface one).

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
