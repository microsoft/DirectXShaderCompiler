// RUN: %dxc -T vs_6_0 -E main %s | FileCheck %s | XFail GitHub #1795

struct S { int x, y; };
int2 main() : OUT
{
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 1)
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 1, i32 2)
    S s = { 1, 2 };
    return (int2)s;
}