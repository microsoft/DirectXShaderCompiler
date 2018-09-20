// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: %{{[a-z0-9]+.*[a-z0-9]*}} = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
// CHECK: %{{[a-z0-9]+.*[a-z0-9]*}} = fmul fast float %{{[a-z0-9]+.*[a-z0-9]*}}, %{{[a-z0-9]+.*[a-z0-9]*}}
// CHECK: %{{[a-z0-9]+.*[a-z0-9]*}} = fmul fast float %{{[a-z0-9]+.*[a-z0-9]*}}, %{{[a-z0-9]+.*[a-z0-9]*}}
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %{{[a-z0-9]+.*[a-z0-9]*}})

float main ( float a : A) : SV_Target
{
    return pow(a, 3);
}