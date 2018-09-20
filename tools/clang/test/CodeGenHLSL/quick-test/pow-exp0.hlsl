// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 3.000000e+00)

float main ( float a : A) : SV_Target
{
    return pow(a, 0) + pow(a, 0.00) + pow(a, -0.00);
}