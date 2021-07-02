// RUN: %dxc -E main -T vs_6_0 -enable-operator-overloading %s | FileCheck %s

// This test checks that when we overload new or delete operator
// dxcompiler generates error and no crashes are observed.

// CHECK: error: overloading 'operator new' is not allowed
// CHECK: error: overloading 'operator delete' is not allowed
// CHECK: error: 'new' is a reserved keyword in HLSL
// CHECK: error: 'delete' is a reserved keyword in HLSL

struct S
{
    float foo;
    void * operator new(int size) {
        return (void *)0;
    }
    void operator delete(void *ptr) {
        (void) ptr;
    }
};

void main() {
    S *a = new S();
    delete a;
}
