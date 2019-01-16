// RUN: %dxc -T vs_6_0 -E main %s | FileCheck %s

row_major int2x2 main() : OUT
{
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 1)
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 1, i32 2)
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 1, i8 0, i32 3)
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 1, i8 1, i32 4)
    int4 v = int4(1, 2, 3, 4);
    return (int2x2)v;
}