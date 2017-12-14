// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// Make sure no fast on fadd
// CHECK: fadd float

float x : register(b0);

float main(float4 col : COLOR) : SV_Target {
    precise float result = x + col;
    return result;
}