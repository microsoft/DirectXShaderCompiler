// RUN: %dxc -T vs_6_0 -E main -fcgl %s -spirv -verify

// expected-error@+1{{'location' attribute only applies to functions, parameters, and fields}}
[[vk::location(0)]]
[[vk::ext_decorate(/* Location */ 30, 1)]]
static float4 output;

void main() {}
