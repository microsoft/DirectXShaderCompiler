// RUN: %dxc -T cs_6_0 -E main - %s -verify
// RUN: %dxc -T cs_6_0 -E main -HV 202x -fcgl %s | FileCheck %s

// Test that the 'auto' keyword can be used as a function return type and
// that the deduced type matches C++14 [dcl.spec.auto] rules.

// CHECK-LABEL: define void @main()

// CHECK-LABEL: define internal <4 x float> @"\01?Scale
// CHECK: ret <4 x float>

// CHECK-LABEL: define internal void @"\01?WriteOutput
// CHECK: ret void

// CHECK-LABEL: define internal i32 @"\01?SquareInt
// CHECK: ret i32

// CHECK-LABEL: define internal float @"\01?SquareFloat
// CHECK: ret float

// CHECK-LABEL: define internal float @"\01?Clamp01
// CHECK: ret float

RWBuffer<float> output : register(u0);

// Deduces int from a single return statement.
// expected-warning@+1 {{'auto' type specifier is a HLSL 202x extension}}
auto SquareInt(int x) {
    return x * x;
}

// Deduces float from a single return statement.
// expected-warning@+1 {{'auto' type specifier is a HLSL 202x extension}}
auto SquareFloat(float x) {
    return x * x;
}

// Deduces float4 from a single return statement.
// expected-warning@+1 {{'auto' type specifier is a HLSL 202x extension}}
auto Scale(float4 v, float s) {
    return v * s;
}

// Deduces void when no return statement is present.
// expected-warning@+1 {{'auto' type specifier is a HLSL 202x extension}}
auto WriteOutput(uint i, float v) {
    output[i] = v;
}

// Multiple return statements with the same deduced type are allowed.
// expected-warning@+1 {{'auto' type specifier is a HLSL 202x extension}}
auto Clamp01(float v) {
    if (v < 0.0f)
        return 0.0f;
    if (v > 1.0f)
        return 1.0f;
    return v;
}

[numthreads(1,1,1)]
void main() {
    float4 v = float4(1, 2, 3, 4);
    float4 s = Scale(v, 0.5f);
    WriteOutput(0, (float)SquareInt(3) + SquareFloat(2.5f) + s.x + Clamp01(1.5f));
}
