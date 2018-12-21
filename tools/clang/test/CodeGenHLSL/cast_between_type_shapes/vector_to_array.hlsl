// RUN: %dxc -T vs_6_0 -E main %s | FileCheck %s | XFail GitHub #1808

typedef int i2[2];
i2 main() : OUT
{
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 1)
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 1, i8 0, i32 2)
    int2 v = int2(1, 2);
    return (i2)v;
}