// RUN: %dxc -T cs_6_0 -HV 202x -verify %s


void voidFunc() {}

Texture2D<float4> tex : register(t0);
Texture2DMS<float4> texMS : register(t1);

// 'string' is an internal type (printf formats, subobject fields); it is only
// declarable as a global, and is intentionally not deducible by 'auto'.
string gStr = "abc";

[numthreads(1,1,1)]
void main() {
    int y = 1;

    // AR_TOBJ_STRING: 'string' is excluded from 'auto' deduction.
    // expected-error@+1 {{'auto' cannot deduce type}}
    auto str = gStr;

    // void: a void-returning call has no value type. 'auto' deduces 'void',
    // which is rejected as an incomplete variable type (not by the HLSL
    // auto-deducibility check, which only special-cases string/inner/subobject).
    // expected-error@+1 {{variable has incomplete type 'void'}}
    auto x = voidFunc();

    // void: an explicit cast to void is equally non-deducible.
    // expected-error@+1 {{variable has incomplete type 'void'}}
    auto v = (void)y;

    // AR_TOBJ_INNER_OBJ: .mips is an inner indexer object (Texture2D::mips_type).
    // expected-error@+1 {{'auto' cannot deduce type}}
    auto m = tex.mips;

    // AR_TOBJ_INNER_OBJ: .sample is an inner indexer object
    // (Texture2DMS::sample_type).
    // expected-error@+1 {{'auto' cannot deduce type}}
    auto s = texMS.sample;
}
