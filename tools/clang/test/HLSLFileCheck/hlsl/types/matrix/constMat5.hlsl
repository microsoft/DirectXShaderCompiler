// RUN: %dxc -E main -T ps_6_2 -enable-16bit-types %s | FileCheck %s

// CHECK: [9 x half] [half 0xH3C00, half 0xH4400, half 0xH4700, half 0xH4000, half 0xH4500, half 0xH4800, half 0xH4200, half 0xH4600, half 0xH4880]

struct Foo {
    half3x3 hmat;
};

Foo fn() {
    Foo foo;
    foo.hmat = float3x3(1, 2, 3, 4, 5, 6, 7, 8, 9);
    return foo;
}

float4 main(float a : A) : SV_Target {
    Foo foo = fn();
    float3 v = float3(a, a * a, a + a);
    return float4(mul(foo.hmat, foo.hmat[a]), 0);
}
