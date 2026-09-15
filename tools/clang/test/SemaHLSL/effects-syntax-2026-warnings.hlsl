// RUN: %dxc -Tlib_6_3 -HV 2021 -verify %s
// RUN: %dxc -Tps_6_0 -HV 2021 -verify %s

Texture2D tex;

Texture2D annotated < int metadata = 1; >; // expected-warning {{possible effect annotation ignored - effect syntax is deprecated and will be removed in HLSL 2026}}

Texture2D stateBlock { Texture = tex; }; // expected-warning {{effect state block ignored - effect syntax is deprecated and will be removed in HLSL 2026}} expected-note {{To use braces as an initializer use them with equal signs}}

sampler legacySampler = sampler_state { Texture = tex; }; // expected-warning {{effect sampler_state assignment ignored - effect syntax is deprecated and will be removed in HLSL 2026}}

BlendState effectObject; // expected-warning {{effect object ignored - effect syntax is deprecated and will be removed in HLSL 2026}}

technique T0 { pass {} } // expected-warning {{effect technique ignored - effect syntax is deprecated and will be removed in HLSL 2026}}

float4 main() : SV_Target {
  return 0;
}
