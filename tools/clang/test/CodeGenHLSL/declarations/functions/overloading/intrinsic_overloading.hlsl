// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s

// Test that intrinsics can be overloaded without
// shadowing the original definition.

// CHECK: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 42)
// CHECK: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 1, i32 1)

struct Struct { int x; };
int abs(Struct s) { return 42; }

int2 main() : OUT
{
    Struct s = { -1 };
    return int2(abs(s), // Should call struct overload
        abs(-1)); // Should call intrinsic
}