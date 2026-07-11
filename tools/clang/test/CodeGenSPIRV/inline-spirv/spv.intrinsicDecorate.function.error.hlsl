// RUN: not %dxc -T cs_6_0 -E main -fcgl %s -spirv 2>&1 | FileCheck %s

// vk::ext_decorate_id and vk::ext_decorate_string decorate a value-id target
// and have no OpFunction-target form, so applying them to a function must be
// diagnosed rather than silently dropped.

[[vk::ext_decorate_string(/* UserTypeGOOGLE */ 5636, "myType")]]
[noinline] uint Identity(uint x) { return x; }

// CHECK: error: vk::ext_decorate_id and vk::ext_decorate_string are not supported on functions

RWStructuredBuffer<uint> buf;

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  buf[0] = Identity(tid.x);
}
