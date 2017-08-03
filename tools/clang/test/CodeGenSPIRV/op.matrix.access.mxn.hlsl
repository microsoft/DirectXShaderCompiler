// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    float2x3 mat;
    float3 vec3;
    float2 vec2;
    float scalar;

    // 1 element (from lvalue)
// CHECK:      [[access0:%\d+]] = OpAccessChain %_ptr_Function_float %mat %int_1 %int_2
// CHECK-NEXT: [[load0:%\d+]] = OpLoad %float [[access0]]
// CHECK-NEXT: OpStore %scalar [[load0]]
    scalar = mat._m12; // Used as rvalue
// CHECK-NEXT: [[load1:%\d+]] = OpLoad %float %scalar
// CHECK-NEXT: [[access1:%\d+]] = OpAccessChain %_ptr_Function_float %mat %int_0 %int_1
// CHECK-NEXT: OpStore [[access1]] [[load1]]
    mat._12 = scalar; // Used as lvalue

    // >1 elements (from lvalue)
// CHECK-NEXT: [[access2:%\d+]] = OpAccessChain %_ptr_Function_float %mat %int_0 %int_1
// CHECK-NEXT: [[load2:%\d+]] = OpLoad %float [[access2]]
// CHECK-NEXT: [[access3:%\d+]] = OpAccessChain %_ptr_Function_float %mat %int_0 %int_2
// CHECK-NEXT: [[load3:%\d+]] = OpLoad %float [[access3]]
// CHECK-NEXT: [[cc0:%\d+]] = OpCompositeConstruct %v2float [[load2]] [[load3]]
// CHECK-NEXT: OpStore %vec2 [[cc0]]
    vec2 = mat._m01_m02; // Used as rvalue
// CHECK-NEXT: [[rhs0:%\d+]] = OpLoad %v3float %vec3
// CHECK-NEXT: [[ce0:%\d+]] = OpCompositeExtract %float [[rhs0]] 0
// CHECK-NEXT: [[access4:%\d+]] = OpAccessChain %_ptr_Function_float %mat %int_1 %int_0
// CHECK-NEXT: OpStore [[access4]] [[ce0]]
// CHECK-NEXT: [[ce1:%\d+]] = OpCompositeExtract %float [[rhs0]] 1
// CHECK-NEXT: [[access5:%\d+]] = OpAccessChain %_ptr_Function_float %mat %int_0 %int_1
// CHECK-NEXT: OpStore [[access5]] [[ce1]]
// CHECK-NEXT: [[ce2:%\d+]] = OpCompositeExtract %float [[rhs0]] 2
// CHECK-NEXT: [[access6:%\d+]] = OpAccessChain %_ptr_Function_float %mat %int_0 %int_0
// CHECK-NEXT: OpStore [[access6]] [[ce2]]
    mat._21_12_11 = vec3; // Used as lvalue

    // 1 element (from rvalue)
// CHECK:      [[cc1:%\d+]] = OpCompositeConstruct %mat2v3float {{%\d+}} {{%\d+}}
// CHECK-NEXT: [[ce3:%\d+]] = OpCompositeExtract %float [[cc1]] 1 2
// CHECK-NEXT: OpStore %scalar [[ce3]]
    // Codegen: construct a temporary matrix first out of (mat + mat) and
    // then extract the value
    scalar = (mat + mat)._m12;

    // > 1 element (from rvalue)
// CHECK:      [[cc2:%\d+]] = OpCompositeConstruct %mat2v3float {{%\d+}} {{%\d+}}
// CHECK-NEXT: [[ce4:%\d+]] = OpCompositeExtract %float [[cc2]] 0 1
// CHECK-NEXT: [[ce5:%\d+]] = OpCompositeExtract %float [[cc2]] 0 2
// CHECK-NEXT: [[cc3:%\d+]] = OpCompositeConstruct %v2float [[ce4]] [[ce5]]
// CHECK-NEXT: OpStore %vec2 [[cc3]]
    // Codegen: construct a temporary matrix first out of (mat * mat) and
    // then extract the value
    vec2 = (mat * mat)._m01_m02;
}
