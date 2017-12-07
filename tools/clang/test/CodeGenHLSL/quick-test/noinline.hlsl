// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Make sure noinline is present
// CHECK: noinline

[noinline]
float foo() {
    return 42;
}
