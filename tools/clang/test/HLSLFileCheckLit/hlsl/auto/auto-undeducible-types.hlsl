// RUN: %dxc -T cs_6_0 -HV 202x -verify %s


void voidFunc() {}

Texture2D<float4> tex : register(t0);
Texture2DMS<float4> texMS : register(t1);

[numthreads(1,1,1)]
void main() {
    int y = 1;

    // AR_TOBJ_VOID: a void-returning call has no value type.
    // expected-error@+1 {{'auto' cannot deduce type 'void'}}
    auto x = voidFunc();

    // AR_TOBJ_VOID: an explicit cast to void is equally non-deducible.
    // expected-error@+1 {{'auto' cannot deduce type 'void'}}
    auto v = (void)y;

    // AR_TOBJ_INNER_OBJ: .mips is an inner indexer object (Texture2D::mips_type).
    // expected-error@+1 {{'auto' cannot deduce type}}
    auto m = tex.mips;

    // AR_TOBJ_INNER_OBJ: .sample is an inner indexer object
    // (Texture2DMS::sample_type).
    // expected-error@+1 {{'auto' cannot deduce type}}
    auto s = texMS.sample;
}
