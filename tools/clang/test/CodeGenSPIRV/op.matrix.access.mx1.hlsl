// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    float3x1 mat;
    float3 vec3;
    float2 vec2;
    float1 vec1;
    float scalar;
    uint index;

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

    // One level indexing (from lvalue)
// CHECK-NEXT: [[index0:%\d+]] = OpLoad %uint %index
// CHECK-NEXT: [[access7:%\d+]] = OpAccessChain %_ptr_Function_float %mat [[index0]]
// CHECK-NEXT: [[load9:%\d+]] = OpLoad %float [[access7]]
// CHECK-NEXT: OpStore %vec1 [[load9]]
    vec1 = mat[index]; // Used as rvalue

    // One level indexing (from lvalue)
// CHECK-NEXT: [[load10:%\d+]] = OpLoad %float %vec1
// CHECK-NEXT: [[access8:%\d+]] = OpAccessChain %_ptr_Function_float %mat %uint_0
// CHECK-NEXT: OpStore [[access8]] [[load10]]
    mat[0] = vec1; // Used as lvalue

    // Two level indexing (from lvalue)
// CHECK-NEXT: [[access9:%\d+]] = OpAccessChain %_ptr_Function_float %mat %uint_1
// CHECK-NEXT: [[load11:%\d+]] = OpLoad %float [[access9]]
// CHECK-NEXT: OpStore %scalar [[load11]]
    scalar = mat[1][index]; // Used as rvalue

    // Two level indexing (from lvalue)
// CHECK-NEXT: [[load12:%\d+]] = OpLoad %float %scalar
// CHECK-NEXT: [[index1:%\d+]] = OpLoad %uint %index
// CHECK-NEXT: [[access10:%\d+]] = OpAccessChain %_ptr_Function_float %mat [[index1]]
// CHECK-NEXT: OpStore [[access10]] [[load12]]
    mat[index][0] = scalar; // Used as lvalue

    // On level indexing (from rvalue)
// CHECK-NEXT: [[load13:%\d+]] = OpLoad %v3float %mat
// CHECK-NEXT: [[load14:%\d+]] = OpLoad %v3float %mat
// CHECK-NEXT: [[add:%\d+]] = OpFAdd %v3float [[load13]] [[load14]]
// CHECK-NEXT: OpStore %temp_var [[add]]
// CHECK-NEXT: [[access11:%\d+]] = OpAccessChain %_ptr_Function_float %temp_var %uint_0
// CHECK-NEXT: [[load15:%\d+]] = OpLoad %float [[access11]]
// CHECK-NEXT: OpStore %vec1 [[load15]]
    vec1 = (mat + mat)[0];

    // Two level indexing (from rvalue)
// CHECK-NEXT: [[index2:%\d+]] = OpLoad %uint %index
// CHECK-NEXT: [[load16:%\d+]] = OpLoad %v3float %mat
// CHECK-NEXT: [[load17:%\d+]] = OpLoad %v3float %mat
// CHECK-NEXT: [[mul:%\d+]] = OpFMul %v3float [[load16]] [[load17]]
// CHECK-NEXT: OpStore %temp_var_0 [[mul]]
// CHECK-NEXT: [[access12:%\d+]] = OpAccessChain %_ptr_Function_float %temp_var_0 [[index2]]
// CHECK-NEXT: [[load18:%\d+]] = OpLoad %float [[access12]]
// CHECK-NEXT: OpStore %scalar [[load18]]
    scalar = (mat * mat)[index][0];
}
