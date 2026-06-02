// RUN: %dxc -T cs_6_0 -HV 202x -verify %s


void voidFunc() {}

Texture2D<float4> tex : register(t0);
Texture2DMS<float4> texMS : register(t1);

[numthreads(1,1,1)]
void main() {
    int y = 1;

    // expected-error@+1 {{'auto' cannot deduce type}}
    auto str = "abc";

    // expected-error@+1 {{variable has incomplete type 'void'}}
    auto x = voidFunc();

    // expected-error@+1 {{variable has incomplete type 'void'}}
    auto v = (void)y;

    // expected-error@+1 {{'auto' cannot deduce type}}
    auto m = tex.mips;

    // expected-error@+1 {{'auto' cannot deduce type}}
    auto s = texMS.sample;
}
