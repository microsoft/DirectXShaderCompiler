// RUN: %dxc -T cs_6_0 -E main -fcgl %s -spirv -verify

// expected-error@+1{{'ext_decorate' cannot be used on a variable already associated with the built-in LocalInvocationId}}
[[vk::ext_decorate(/* BuiltIn */ 11, /* WorkgroupId */ 26)]]
[[vk::ext_decorate(/* BuiltIn */ 11, /* LocalInvocationId */ 27)]]
[[vk::ext_storage_class(/* Input */ 1)]]
static uint3 invalid;

[numthreads(1, 1, 1)]
void main() { }
