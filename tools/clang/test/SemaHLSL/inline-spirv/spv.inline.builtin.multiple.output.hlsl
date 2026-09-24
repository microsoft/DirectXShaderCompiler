// RUN: %dxc -T cs_6_0 -E main -fcgl %s -spirv -verify

// expected-error@+1{{'ext_builtin_output' cannot be used on a variable already associated with the built-in WorkgroupId}}
[[vk::ext_builtin_output(/* NumWorkgroups */ 24)]]
[[vk::ext_builtin_output(/* WorkgroupId */ 26)]]
static uint3 invalid;

[numthreads(1, 1, 1)]
void main() { }
