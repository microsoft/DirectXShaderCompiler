// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    float1x1 mat;
    float3 vec3;
    float2 vec2;
    float scalar;

    // 1 element (from lvalue)
// CHECK:      [[load0:%\d+]] = OpLoad %float %mat
// CHECK-NEXT: OpStore %scalar [[load0]]
    scalar = mat._m00; // Used as rvalue
// CHECK-NEXT: [[load1:%\d+]] = OpLoad %float %scalar
// CHECK-NEXT: OpStore %mat [[load1]]
    mat._11 = scalar; // Used as lvalue

    // >1 elements (from lvalue)
// CHECK-NEXT: [[load2:%\d+]] = OpLoad %float %mat
// CHECK-NEXT: [[load3:%\d+]] = OpLoad %float %mat
// CHECK-NEXT: [[cc0:%\d+]] = OpCompositeConstruct %v2float [[load2]] [[load3]]
// CHECK-NEXT: OpStore %vec2 [[cc0]]
    vec2 = mat._11_11; // Used as rvalue

    // The following statements will trigger errors:
    //   invalid format for vector swizzle
    // scalar = (mat + mat)._m00;
    // vec2 = (mat * mat)._11_11;
}
