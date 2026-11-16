// RUN: %dxc -T vs_6_0 -E main -fcgl %s -spirv -verify

// expected-error@+1{{decoration Location requires 1 operand}}
[[vk::ext_decorate(/* Location */ 30)]]
[[vk::ext_decorate(/* Location */ 30, 1)]]
static float4 output;

void main() {}
