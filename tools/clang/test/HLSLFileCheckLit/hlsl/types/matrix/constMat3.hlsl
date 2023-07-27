// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s --check-prefix=CHECK --check-prefix=COL
// RUN: %dxc -E main -T ps_6_0 -Zpr %s | FileCheck %s --check-prefix=CHECK --check-prefix=ROW

// COL: [9 x float] [float 1.000000e+00, float 4.000000e+00, float 7.000000e+00, float 2.000000e+00, float 5.000000e+00, float 8.000000e+00, float 3.000000e+00, float 6.000000e+00, float 9.000000e+00]
// ROW: [9 x float] [float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00, float 5.000000e+00, float 6.000000e+00, float 7.000000e+00, float 8.000000e+00, float 9.000000e+00]
// CHECK: fmul {{.+}}, 2.000000e+00
// CHECK: fmul {{.+}}, 3.000000e+00
// CHECK: %[[IDX:[a-z0-9A-Z]+]] = fptoui float %1 to i32
// COL: add i32 %[[IDX]], 3
// COL: add i32 %[[IDX]], 6
// ROW: %[[BASE:[a-z0-9A-Z]+]] = mul i32 %[[IDX]], 3
// ROW: add i32 %[[BASE]], 1
// ROW: add i32 %[[BASE]], 2

static const float3x3 g_mat1 = {
    1, 2, 3,
    4, 5, 6,
    7, 8, 9,
};

float4 main(float a : A) : SV_Target {
    float3 v = float3(a, 0, 0);
    float4 c = float4(mul(v, g_mat1), 0);
    return c + float4(g_mat1[a],0);
}
