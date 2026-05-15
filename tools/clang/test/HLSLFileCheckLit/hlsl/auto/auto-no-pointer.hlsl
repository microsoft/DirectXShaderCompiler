// RUN: not %dxc -T cs_6_0 %s 2>&1 | FileCheck %s

// Test that 'auto' cannot be used to declare pointer types in HLSL.
// Pointers are unsupported in HLSL.

// CHECK: error: pointers are unsupported in HLSL

[numthreads(1,1,1)]
void main() {
    int x = 1;
    auto* ptr = &x;
}
