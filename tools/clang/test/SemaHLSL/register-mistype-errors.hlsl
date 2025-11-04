// RUN: %dxc -T lib_6_3 -verify %s

// expected-error@+1 {{Syntax similar to : register() was used but unexpected keyword 'registers' was used instead.}}
RWStructuredBuffer<float4> uav1 : registers(u3);

[shader("pixel")]
float4 main(): SV_Target
{
  uav1[0] = 2;
  return 0.xxxx;
}
