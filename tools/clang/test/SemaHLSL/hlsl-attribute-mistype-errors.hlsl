// RUN: %dxc -T lib_6_3 -verify %s

// expected-error@+1 {{Syntax indicated an hlsl attribute (: packoffset() or : register()), but unexpected attribute 'registers' was used instead.}}
RWStructuredBuffer<float4> uav1 : registers(u3);

// expected-error@+1 {{Syntax indicated an hlsl attribute (: packoffset() or : register()), but unexpected attribute 'registers' was used instead.}}
RWStructuredBuffer<float4> uav2 : registers(outer_space);

// expected-error@+1 {{Syntax indicated an hlsl attribute (: packoffset() or : register()), but unexpected attribute 'UNDEFINED_MACRO1' was used instead.}}
RWStructuredBuffer<float4> uav3 : UNDEFINED_MACRO1(u3);

// expected-error@+1 {{Syntax indicated an hlsl attribute (: packoffset() or : register()), but unexpected attribute 'UNDEFINED_MACRO' was used instead.}}
RWStructuredBuffer<float4> uav4 : UNDEFINED_MACRO(something, more, complex);

cbuffer buf {

  // expected-error@+1 {{Syntax indicated an hlsl attribute (: packoffset() or : register()), but unexpected attribute 'packoffsets' was used instead.}}
  float4 v0 : packoffsets(c0);

  // expected-error@+1 {{Syntax indicated an hlsl attribute (: packoffset() or : register()), but unexpected attribute 'packoffsets' was used instead.}}
  float4 v1 : packoffsets(invalid_syntax);

  // expected-error@+1 {{Syntax indicated an hlsl attribute (: packoffset() or : register()), but unexpected attribute 'UNDEFINED_MACRO2' was used instead.}}
  float v2 : UNDEFINED_MACRO2(c0.w);

  // expected-error@+1 {{Syntax indicated an hlsl attribute (: packoffset() or : register()), but unexpected attribute 'UNDEFINED_MACRO' was used instead.}}
  float v3 : UNDEFINED_MACRO(something, more, complex);
};

[shader("pixel")]
float4 main(): SV_Target
{
  uav1[0] = v0;
  uav2[0] = v1;
  uav3[0] = v2;
  uav4[0] = v3;
  return 0.xxxx;
}
