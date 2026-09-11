// RUN: %dxc -T vs_6_9 -E main -spirv %s -verify

// expected-error@+1{{'ext_storage_class' cannot be used on a variable already associated to another storage class Output}}
[[vk::ext_storage_class(/* Input */ 1)]]
[[vk::ext_builtin_output(/* Position */ 0)]]
static float4 output;

void main() {
}

