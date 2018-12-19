// RUN: %dxc /T vs_6_0 /E main %s | FileCheck %s

// Test reading input matrices in both orientations.

int main(row_major int4x4 r : R, column_major int4x4 c : C) : OUT
{
    // Unfortunately the loads get reordered here.
    // CHECK: call i32 @dx.op.loadInput.i32(i32 4, i32 1, i32 2, i8 1, i32 undef)
    // CHECK: call i32 @dx.op.loadInput.i32(i32 4, i32 0, i32 1, i8 2, i32 undef)
    return r._23 + c._23;
}