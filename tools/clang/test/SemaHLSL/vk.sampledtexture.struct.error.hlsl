// RUN: %dxc -T ps_6_0 -E main %s -spirv -fcgl -verify

struct Struct { float f; };

vk::SampledTexture2D<Struct> t;

float4 main(float2 f2 : F2) : SV_TARGET
{
    return t.Sample(f2); // expected-error {{cannot Sample from resource containing}} expected-error {{cannot initialize return object}}
}
