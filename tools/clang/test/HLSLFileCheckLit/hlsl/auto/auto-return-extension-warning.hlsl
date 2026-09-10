// RUN: %dxc -T cs_6_0 -E main -HV 2016 -verify %s
// RUN: %dxc -T cs_6_0 -E main -HV 2017 -verify %s
// RUN: %dxc -T cs_6_0 -E main -HV 2018 -verify %s
// RUN: %dxc -T cs_6_0 -E main -HV 2021 -verify %s

// 'auto' is allowed as a function return type from HLSL 2016 onward, but
// using it before language mode 202x produces an extension warning.

RWBuffer<float> output : register(u0);

// expected-warning@+1 {{'auto' type specifier is a HLSL 202x extension}}
auto Square(int x) {
    return x * x;
}

[numthreads(1,1,1)]
void main() {
    output[0] = (float)Square(3);
}
