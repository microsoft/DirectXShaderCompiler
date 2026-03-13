// RUN: %dxc -T cs_6_0 -E main -fcgl %s -spirv -verify

// expected-error@+1{{decoration BuiltIn requires 1 operand}}
[[vk::ext_decorate(/* BuiltIn */ 11)]]
static uint3 input;

[numthreads(1, 1, 1)]
void main() {}
