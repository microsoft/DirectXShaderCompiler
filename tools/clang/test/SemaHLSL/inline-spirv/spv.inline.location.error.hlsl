// RUN: %dxc -T vs_6_0 -E main -fcgl %s -spirv -verify

// expected-error@+1{{invalid 'ext_decorate' : target already associated with another location 1}}
[[vk::ext_decorate(/* Location */ 30, 0)]]
[[vk::ext_decorate(/* Location */ 30, 1)]]
static float4 output;

void main() {}
