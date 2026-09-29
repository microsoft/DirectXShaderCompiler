// RUN: %dxc -T vs_6_0 -E main -verify %s -spirv

cbuffer MyCBuffer {
    float a = 1.0; // expected-warning{{initializer for a variable in a cbuffer will be ignored}}
    float4 b = 2.0; // expected-warning{{initializer for a variable in a cbuffer will be ignored}}
};

float main() : A {
    return 1.0;
}