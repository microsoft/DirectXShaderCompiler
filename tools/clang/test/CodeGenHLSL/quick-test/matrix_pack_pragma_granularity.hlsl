// RUN: %dxc /T vs_6_0 /E main > %s | FileCheck %s | XFail GitHub #1798

// Tests the exact place at which #pragma pack_matrix takes effect.
// To be extremely pedantic, FXC actually applies it at the final '>'
// token in a long-form matrix<int, 2, 2> declaration.

#pragma pack_matrix(column_major)

typedef
#pragma pack_matrix(row_major)
int2x2
#pragma pack_matrix(column_major)
i22;

void main(out i22 mat : OUT)
{
    // CHECK: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 11)
    // CHECK: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 12)
    // CHECK: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 21)
    // CHECK: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 22)
    mat = i22(11, 12, 21, 22);

    // FXC output, for reference:
    // mov o0.xy, l(11,12,0,0)
    // mov o1.xy, l(21,22,0,0)
}
