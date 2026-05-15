// RUN: not %dxc -T cs_6_0 -HV 202x %s 2>&1 | FileCheck %s

// Test that 'auto' cannot be used to declare reference types in HLSL.
// References are unsupported in HLSL.

// CHECK: error: references are unsupported in HLSL

int gVal = 42;

[numthreads(1,1,1)]
void main() {
    auto& ref = gVal;
}
