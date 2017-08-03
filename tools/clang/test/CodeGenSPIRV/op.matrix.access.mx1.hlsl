// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    float3x1 mat;
    float3 vec3;
    float2 vec2;
    float scalar;

    // 1 element (from lvalue)
// CHECK:      [[access0:%\d+]] = OpAccessChain %_ptr_Function_float %mat %int_2
// CHECK-NEXT: [[load0:%\d+]] = OpLoad %float [[access0]]
// CHECK-NEXT: OpStore %scalar [[load0]]
    scalar = mat._m20; // Used as rvalue
// CHECK-NEXT: [[load1:%\d+]] = OpLoad %float %scalar
// CHECK-NEXT: [[access1:%\d+]] = OpAccessChain %_ptr_Function_float %mat %int_1
// CHECK-NEXT: OpStore [[access1]] [[load1]]
    mat._21 = scalar; // Used as lvalue

    // > 1 elements (from lvalue)
// CHECK-NEXT: [[access2:%\d+]] = OpAccessChain %_ptr_Function_float %mat %int_0
// CHECK-NEXT: [[load2:%\d+]] = OpLoad %float [[access2]]
// CHECK-NEXT: [[access3:%\d+]] = OpAccessChain %_ptr_Function_float %mat %int_2
// CHECK-NEXT: [[load3:%\d+]] = OpLoad %float [[access3]]
// CHECK-NEXT: [[access4:%\d+]] = OpAccessChain %_ptr_Function_float %mat %int_1
// CHECK-NEXT: [[load4:%\d+]] = OpLoad %float [[access4]]
// CHECK-NEXT: [[cc0:%\d+]] = OpCompositeConstruct %v3float [[load2]] [[load3]] [[load4]]
// CHECK-NEXT: OpStore %vec3 [[cc0]]
    vec3 = mat._11_31_21; // Used as rvalue
// CHECK-NEXT: [[rhs0:%\d+]] = OpLoad %v2float %vec2
// CHECK-NEXT: [[ce0:%\d+]] = OpCompositeExtract %float [[rhs0]] 0
// CHECK-NEXT: [[access5:%\d+]] = OpAccessChain %_ptr_Function_float %mat %int_0
// CHECK-NEXT: OpStore [[access5]] [[ce0]]
// CHECK-NEXT: [[ce1:%\d+]] = OpCompositeExtract %float [[rhs0]] 1
// CHECK-NEXT: [[access6:%\d+]] = OpAccessChain %_ptr_Function_float %mat %int_2
// CHECK-NEXT: OpStore [[access6]] [[ce1]]
    mat._m00_m20 = vec2; // Used as lvalue

    // 1 element (from rvalue)
// CHECK-NEXT: [[load5:%\d+]] = OpLoad %v3float %mat
// CHECK-NEXT: [[load6:%\d+]] = OpLoad %v3float %mat
// CHECK-NEXT: [[add0:%\d+]] = OpFAdd %v3float [[load5]] [[load6]]
// CHECK-NEXT: [[ce2:%\d+]] = OpCompositeExtract %float [[add0]] 2
// CHECK-NEXT: OpStore %scalar [[ce2]]
    // Codegen: construct a temporary vector first out of (mat + mat) and
    // then extract the value
    scalar = (mat + mat)._m20;

    // > 1 element (from rvalue)
// CHECK-NEXT: [[load7:%\d+]] = OpLoad %v3float %mat
// CHECK-NEXT: [[load8:%\d+]] = OpLoad %v3float %mat
// CHECK-NEXT: [[mul0:%\d+]] = OpFMul %v3float [[load7]] [[load8]]
// CHECK-NEXT: [[ce3:%\d+]] = OpCompositeExtract %float [[mul0]] 0
// CHECK-NEXT: [[ce4:%\d+]] = OpCompositeExtract %float [[mul0]] 1
// CHECK-NEXT: [[cc1:%\d+]] = OpCompositeConstruct %v2float [[ce3]] [[ce4]]
// CHECK-NEXT: OpStore %vec2 [[cc1]]
    // Codegen: construct a temporary vector first out of (mat * mat) and
    // then extract the value
    vec2 = (mat * mat)._11_21;
}
