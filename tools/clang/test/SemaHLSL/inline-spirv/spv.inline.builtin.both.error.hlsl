// RUN: %dxc -T ps_6_0 -E main -fcgl %s -spirv -verify

// expected-error@+1{{'ext_builtin_input' cannot be used on a variable already associated to another storage class Output}}
[[vk::ext_builtin_input(/* NumWorkgroups */ 24)]]
[[vk::ext_builtin_output(/* NumWorkgroups */ 24)]]
static uint3 invalid;

void main() {
}
