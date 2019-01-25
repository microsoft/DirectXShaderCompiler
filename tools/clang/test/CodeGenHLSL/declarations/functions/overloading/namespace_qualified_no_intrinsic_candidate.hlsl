// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s

// Regression test for GitHub #1884, where intrinsics were considered
// valid overload candidates for overloaded functions of the same name in a namespace

// CHECK: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 42)

namespace foo
{
    int abs(int2 x) { return 42; }
    int abs() { return 0; }
}

int main() : OUT
{
    // This should not consider the abs(int) intrinsic from the global namespace,
    // regardless of the fact that it is a better match for the arguments.
    return foo::abs(0);
}