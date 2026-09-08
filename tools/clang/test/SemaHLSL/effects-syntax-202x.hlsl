// RUN: %dxc -Tlib_6_3 -HV 202x -verify %s
// RUN: %dxc -Tps_6_0 -HV 202x -verify %s

// The legacy HLSL effects syntax is removed in HLSL 202x. Constructs that
// earlier language versions silently ignored (with an effects-syntax warning)
// must now produce natural diagnostics. This test verifies the parser and
// semantic-analysis phases reject the various effects-syntax forms.

// Register and packoffset annotations are still valid and must keep working.
Texture2D tex : register(t1);
SamplerState samLinear : register(s7);

// Effect annotation '< ... >' on a declarator is no longer skipped.
Texture2D texAnnotated < int foo = 1; >;  /* expected-error {{expected ';' after top level declarator}} expected-error {{expected unqualified-id}} */

// Effect state block '{ ... }' after a declarator is no longer skipped.
Texture2D texStateBlock { state = foo; };  /* expected-error {{expected ';' after top level declarator}} */

// sampler_state assignment initializer is no longer skipped.
sampler S : register(s1) = sampler_state { texture = tex; };  /* expected-error {{expected expression}} */

// Deprecated effect object type names are no longer declared, so they are
// reported as unknown types.
BlendState bs;             /* expected-error {{unknown type name 'BlendState'}} */
DepthStencilState dss;     /* expected-error {{unknown type name 'DepthStencilState'}} */
RasterizerState rs;        /* expected-error {{unknown type name 'RasterizerState'}} */
RenderTargetView rtv;      /* expected-error {{unknown type name 'RenderTargetView'}} */
DepthStencilView dsv;      /* expected-error {{unknown type name 'DepthStencilView'}} */
PixelShader ps;            /* expected-error {{unknown type name 'PixelShader'}} */
VertexShader vs;           /* expected-error {{unknown type name 'VertexShader'}} */
GeometryShader gs;         /* expected-error {{unknown type name 'GeometryShader'}} */
HullShader hs;             /* expected-error {{unknown type name 'HullShader'}} */
DomainShader ds;           /* expected-error {{unknown type name 'DomainShader'}} */
ComputeShader cs;          /* expected-error {{unknown type name 'ComputeShader'}} */
texture texLegacy;         /* expected-error {{unknown type name 'texture'}} */
pixelfragment pfrag;       /* expected-error {{unknown type name 'pixelfragment'}} */
vertexfragment vfrag;      /* expected-error {{unknown type name 'vertexfragment'}} */

// The 'technique' keyword is no longer treated as a legacy effects block.
technique T0 { pass {} }   /* expected-error {{expected unqualified-id}} */

[shader("pixel")]
float4 main() : SV_Target {
  return tex.Sample(samLinear, float2(0.1, 0.2));
}
