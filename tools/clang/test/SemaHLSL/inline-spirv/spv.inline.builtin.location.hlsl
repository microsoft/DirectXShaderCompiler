// RUN: %dxc -T cs_6_0 -E main -fcgl %s -spirv -verify

[[vk::ext_decorate(/* BuiltIn */ 11, /* WorkgroupId */ 26)]]
[[vk::ext_builtin_input(/* WorkgroupId */ 26)]]
[[vk::ext_decorate(/* Location */ 30, 0)]]
// expected-error@+1{{Location incompatible with the BuiltIn WorkgroupId}}
static uint3 input;

[numthreads(1, 1, 1)]
void main() { }
