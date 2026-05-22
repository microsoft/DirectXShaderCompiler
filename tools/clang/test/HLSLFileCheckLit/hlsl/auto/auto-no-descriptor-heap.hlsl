// RUN: %dxc -T ps_6_6 -HV 202x -verify %s

// Test that 'auto' cannot be used to deduce the type of ResourceDescriptorHeap
// or SamplerDescriptorHeap indexed values. Users must spell out a concrete
// resource or sampler type so the heap entry can be cast to the intended kind.

int someFunc() { return 7; }

float4 main(float4 pos : SV_Position) : SV_Target {
    // Indexed heap access -- the operator[] peel finds the heap DeclRefExpr.
    // expected-error@+1 {{'auto' cannot deduce the type of 'ResourceDescriptorHeap'}}
    auto tex = ResourceDescriptorHeap[0];
    // expected-error@+1 {{'auto' cannot deduce the type of 'SamplerDescriptorHeap'}}
    auto samp = SamplerDescriptorHeap[0];

    // Bare heap reference -- no subscript, plain DeclRefExpr.
    // expected-error@+1 {{'auto' cannot deduce the type of 'ResourceDescriptorHeap'}}
    auto heap = ResourceDescriptorHeap;
    // expected-error@+1 {{'auto' cannot deduce the type of 'SamplerDescriptorHeap'}}
    auto sheap = SamplerDescriptorHeap;

    // Parenthesized -- IgnoreParenImpCasts strips the ParenExpr.
    // expected-error@+1 {{'auto' cannot deduce the type of 'SamplerDescriptorHeap'}}
    auto pSamp = (SamplerDescriptorHeap[0]);
    // expected-error@+1 {{'auto' cannot deduce the type of 'ResourceDescriptorHeap'}}
    auto pHeap = (ResourceDescriptorHeap);

    // Negative cases -- auto deduces a regular type; no diagnostic expected.
    auto sum = 1 + 2;
    auto called = someFunc();
    Texture2D<float4> tex2 = ResourceDescriptorHeap[1];
    auto sampled = tex2.Sample((SamplerState)SamplerDescriptorHeap[0], pos.xy);

    return float4(0, 0, 0, 0);
}
