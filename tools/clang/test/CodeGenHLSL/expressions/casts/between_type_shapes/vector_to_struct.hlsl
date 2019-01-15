// RUN: %dxc -T vs_6_0 -E main %s | FileCheck %s

struct S { int x, y; };
S main() : OUT
{
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 1)
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 1, i32 0, i8 0, i32 2)
    int2 v = int2(1, 2);
    return (S)v;
}