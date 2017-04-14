// RUN: %dxc -Emain -Tps_6_0 %s | %FileCheck %s
// CHECK: alloca [3 x i32]

// We could get hoist the arrays individually but the two allocas are
// merged by inlining and that prevents hoisting. It becomes a good
// negative test because different constants are written to the alloca.

int foo(int i) {
    int A[] = {1,2,3};
    return A[i];
}

int bar(int i) {
    int A[] = {4,5,6};
    return A[i];
}

int main(int i : I) : SV_Target {
    return foo(i) + bar(i);
}
