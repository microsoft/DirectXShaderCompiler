// RUN: %dxc -T cs_6_9 -E main -spirv %s -verify

// expected-error@+1{{'ext_storage_class' cannot be used on a variable already associated to another storage class Input}}
[[vk::ext_storage_class(/* Output */ 3)]]
[[vk::ext_builtin_input(/* WorkgroupId */ 26)]]
static uint3 input;

[numthreads(1, 1, 1)]
void main() { }
