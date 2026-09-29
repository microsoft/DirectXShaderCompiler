// RUN: %dxc -T ps_6_0 -E main -HV 202x -verify %s

// Verify that const-declared local resource objects cannot be reassigned.
// All built-in HLSL resource/handle types should reject assignment when
// declared const, but their (const) instance methods must remain callable.

Texture2D<float4>     tex  : register(t0);
Texture2D<float4>     tex2 : register(t1);
SamplerState          samp : register(s0);
RWBuffer<float4>      buf  : register(u0);
RWBuffer<float4>      buf2 : register(u1);

float4 main(float2 uv : TEXCOORD) : SV_Target {
  const Texture2D<float4> ltex = tex; // expected-note{{variable 'ltex' declared const here}}
  const RWBuffer<float4>  lbuf = buf; // expected-note{{variable 'lbuf' declared const here}}

  // Reassigning a const resource is an error.
  ltex = tex2; // expected-error{{cannot assign to variable 'ltex' with const-qualified type 'const Texture2D<float4>'}}
  lbuf = buf2; // expected-error{{cannot assign to variable 'lbuf' with const-qualified type 'const RWBuffer<float4>'}}

  // But const methods on the handle are fine.
  float4 v = ltex.Sample(samp, uv);
  return v + lbuf.Load(0);
}
