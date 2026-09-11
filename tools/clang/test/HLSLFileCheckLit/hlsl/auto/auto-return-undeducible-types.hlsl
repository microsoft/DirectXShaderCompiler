// RUN: %dxc -T lib_6_6 -HV 202x -verify %s

// Test that deducing a return type that 'auto' cannot represent produces a
// diagnostic, mirroring the checks already performed for 'auto' variables
// (see auto-undeducible-types.hlsl and auto-no-subobject.hlsl).

Texture2D<float4> tex : register(t0);
Texture2DMS<float4> texMS : register(t1);

GlobalRootSignature grs = {"CBV(b0)"};

// String literals cannot be deduced by 'auto'.
auto GetString() {
    // expected-error@+1 {{'auto' cannot deduce type 'literal string'}}
    return "abc";
}

// The proxy types used for '.mips'/'.sample' subscript operators cannot be
// deduced by 'auto'.
auto GetMips() {
    // expected-error@+1 {{'auto' cannot deduce type}}
    return tex.mips;
}

auto GetMipsElement() {
    // expected-error@+1 {{'auto' cannot deduce type}}
    return tex.mips[0];
}

auto GetSample() {
    // expected-error@+1 {{'auto' cannot deduce type}}
    return texMS.sample;
}

auto GetSampleElement() {
    // expected-error@+1 {{'auto' cannot deduce type}}
    return texMS.sample[0];
}

// Subobjects cannot be deduced by 'auto'.
auto GetSubobject() {
    // expected-error@+1 {{'auto' cannot deduce type 'GlobalRootSignature'}}
    return grs;
}

// Fully subscripted mips/sample accesses deduce a normal, deducible type.
auto GetMipsValue() {
    return tex.mips[0][int2(1, 2)];
}

auto GetSampleValue() {
    return texMS.sample[0][int2(1, 2)];
}

auto GetDynamicResource() {
  // expected-error@+1 {{'auto' cannot deduce type '.Resource'}}
  return ResourceDescriptorHeap[0];
}

auto GetDynamicSampler() {
  // expected-error@+1 {{'auto' cannot deduce type '.Sampler'}}
  return SamplerDescriptorHeap[0];
}
