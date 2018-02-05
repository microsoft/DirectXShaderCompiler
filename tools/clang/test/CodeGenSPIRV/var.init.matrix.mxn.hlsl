// Run: %dxc -T ps_6_0 -E main

// TODO: optimize to generate constant composite for suitable initializers
// TODO: decompose matrix in initializer

// CHECK:      [[v3fc1:%\d+]] = OpConstantComposite %v3float %float_1 %float_1 %float_1
// CHECK-NEXT: [[v3fc0:%\d+]] = OpConstantComposite %v3float %float_0 %float_0 %float_0

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    // Constructor
// CHECK:      [[cc00:%\d+]] = OpCompositeConstruct %v3float %float_1 %float_2 %float_3
// CHECK-NEXT: [[cc01:%\d+]] = OpCompositeConstruct %v3float %float_4 %float_5 %float_6
// CHECK-NEXT: [[cc02:%\d+]] = OpCompositeConstruct %mat2v3float [[cc00]] [[cc01]]
// CHECK-NEXT: OpStore %mat1 [[cc02]]
    float2x3 mat1 = float2x3(1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
    // All elements in a single {}
// CHECK-NEXT: [[cc03:%\d+]] = OpCompositeConstruct %v2float %float_1 %float_2
// CHECK-NEXT: [[cc04:%\d+]] = OpCompositeConstruct %v2float %float_3 %float_4
// CHECK-NEXT: [[cc05:%\d+]] = OpCompositeConstruct %v2float %float_5 %float_6
// CHECK-NEXT: [[cc06:%\d+]] = OpCompositeConstruct %mat3v2float [[cc03]] [[cc04]] [[cc05]]
// CHECK-NEXT: OpStore %mat2 [[cc06]]
    float3x2 mat2 = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    // Each vector has its own {}
// CHECK-NEXT: [[cc07:%\d+]] = OpCompositeConstruct %v3float %float_1 %float_2 %float_3
// CHECK-NEXT: [[cc08:%\d+]] = OpCompositeConstruct %v3float %float_4 %float_5 %float_6
// CHECK-NEXT: [[cc09:%\d+]] = OpCompositeConstruct %mat2v3float [[cc07]] [[cc08]]
// CHECK-NEXT: OpStore %mat3 [[cc09]]
    float2x3 mat3 = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
    // Wired & complicated {}s
// CHECK-NEXT: [[cc10:%\d+]] = OpCompositeConstruct %v2float %float_1 %float_2
// CHECK-NEXT: [[cc11:%\d+]] = OpCompositeConstruct %v2float %float_3 %float_4
// CHECK-NEXT: [[cc12:%\d+]] = OpCompositeConstruct %v2float %float_5 %float_6
// CHECK-NEXT: [[cc13:%\d+]] = OpCompositeConstruct %mat3v2float [[cc10]] [[cc11]] [[cc12]]
// CHECK-NEXT: OpStore %mat4 [[cc13]]
    float3x2 mat4 = {{1.0}, {2.0, 3.0}, 4.0, {{5.0}, {{6.0}}}};

    float scalar;
    float1 vec1;
    float2 vec2;
    float3 vec3;
    float4 vec4;

    // Mixed scalar and vector
// CHECK-NEXT: [[s:%\d+]] = OpLoad %float %scalar
// CHECK-NEXT: [[vec1:%\d+]] = OpLoad %float %vec1
// CHECK-NEXT: [[vec2:%\d+]] = OpLoad %v2float %vec2
// CHECK-NEXT: [[ce00:%\d+]] = OpCompositeExtract %float [[vec2]] 0
// CHECK-NEXT: [[ce01:%\d+]] = OpCompositeExtract %float [[vec2]] 1
// CHECK-NEXT: [[cc14:%\d+]] = OpCompositeConstruct %v4float [[s]] [[vec1]] [[ce00]] [[ce01]]

// CHECK-NEXT: [[vec3:%\d+]] = OpLoad %v3float %vec3
// CHECK-NEXT: [[ce02:%\d+]] = OpCompositeExtract %float [[vec3]] 0
// CHECK-NEXT: [[ce03:%\d+]] = OpCompositeExtract %float [[vec3]] 1
// CHECK-NEXT: [[ce04:%\d+]] = OpCompositeExtract %float [[vec3]] 2
// CHECK-NEXT: [[vec2a:%\d+]] = OpLoad %v2float %vec2
// CHECK-NEXT: [[ce05:%\d+]] = OpCompositeExtract %float [[vec2a]] 0
// CHECK-NEXT: [[ce06:%\d+]] = OpCompositeExtract %float [[vec2a]] 1
// CHECK-NEXT: [[cc15:%\d+]] = OpCompositeConstruct %v4float [[ce02]] [[ce03]] [[ce04]] [[ce05]]

// CHECK-NEXT: [[cc16:%\d+]] = OpCompositeConstruct %v4float [[ce06]] %float_1 %float_2 %float_3

// CHECK-NEXT: [[vec4:%\d+]] = OpLoad %v4float %vec4

// CHECK-NEXT: [[cc17:%\d+]] = OpCompositeConstruct %mat4v4float [[cc14]] [[cc15]] [[cc16]] [[vec4]]
// CHECK-NEXT:  OpStore %mat5 [[cc17]]
    float4x4 mat5 = {scalar, vec1, vec2,  // [0]
                     vec3, vec2,          // [1] + 1 scalar
                     float2(1., 2.), 3.,  // [2] - 1 scalar
                     vec4                 // [3]
    };

    // From value of the same type
// CHECK-NEXT: [[mat5:%\d+]] = OpLoad %mat4v4float %mat5
// CHECK-NEXT: OpStore %mat6 [[mat5]]
    float4x4 mat6 = float4x4(mat5);

    int intScalar;
    uint uintScalar;
    bool boolScalar;
    int1 intVec1;
    uint2 uintVec2;
    bool3 boolVec3;

    // Casting
// CHECK-NEXT: [[intvec1:%\d+]] = OpLoad %int %intVec1
// CHECK-NEXT: [[convert0:%\d+]] = OpConvertSToF %float [[intvec1]]
// CHECK-NEXT: [[uintscalar:%\d+]] = OpLoad %uint %uintScalar
// CHECK-NEXT: [[convert1:%\d+]] = OpConvertUToF %float [[uintscalar]]
// CHECK-NEXT: [[uintvec2:%\d+]] = OpLoad %v2uint %uintVec2
// CHECK-NEXT: [[ce07:%\d+]] = OpCompositeExtract %uint [[uintvec2]] 0
// CHECK-NEXT: [[ce08:%\d+]] = OpCompositeExtract %uint [[uintvec2]] 1
// CHECK-NEXT: [[convert2:%\d+]] = OpConvertUToF %float [[ce07]]
// CHECK-NEXT: [[cc18:%\d+]] = OpCompositeConstruct %v3float [[convert0]] [[convert1]] [[convert2]]

// CHECK-NEXT: [[convert3:%\d+]] = OpConvertUToF %float [[ce08]]
// CHECK-NEXT: [[intscalar:%\d+]] = OpLoad %int %intScalar
// CHECK-NEXT: [[convert4:%\d+]] = OpConvertSToF %float [[intscalar]]
// CHECK-NEXT: [[boolscalar:%\d+]] = OpLoad %bool %boolScalar
// CHECK-NEXT: [[convert5:%\d+]] = OpSelect %float [[boolscalar]] %float_1 %float_0
// CHECK-NEXT: [[cc19:%\d+]] = OpCompositeConstruct %v3float [[convert3]] [[convert4]] [[convert5]]

// CHECK-NEXT: [[boolvec3:%\d+]] = OpLoad %v3bool %boolVec3
// CHECK-NEXT: [[convert6:%\d+]] = OpSelect %v3float [[boolvec3]] [[v3fc1]] [[v3fc0]]
// CHECK-NEXT: [[cc20:%\d+]] = OpCompositeConstruct %mat3v3float [[cc18]] [[cc19]] [[convert6]]

// CHECK-NEXT: OpStore %mat7 [[cc20]]
    float3x3 mat7 = {intVec1, uintScalar, uintVec2, // [0] + 1 scalar
                     intScalar, boolScalar,         // [1] - 1 scalar
                     boolVec3                       // [2]
    };

    // Decomposing matrices
    float2x2 mat8;
    float2x4 mat9;
    float4x1 mat10;
    // TODO: Optimization opportunity. We are extracting all elements in each
    // vector and then reconstructing the original vector. Optimally we should
    // extract vectors from matrices directly.

// CHECK-NEXT: [[mat8:%\d+]] = OpLoad %mat2v2float %mat8
// CHECK-NEXT: [[mat8_00:%\d+]] = OpCompositeExtract %float [[mat8]] 0 0
// CHECK-NEXT: [[mat8_01:%\d+]] = OpCompositeExtract %float [[mat8]] 0 1
// CHECK-NEXT: [[mat8_10:%\d+]] = OpCompositeExtract %float [[mat8]] 1 0
// CHECK-NEXT: [[mat8_11:%\d+]] = OpCompositeExtract %float [[mat8]] 1 1
// CHECK-NEXT: [[cc21:%\d+]] = OpCompositeConstruct %v4float [[mat8_00]] [[mat8_01]] [[mat8_10]] [[mat8_11]]

// CHECK-NEXT: [[mat9:%\d+]] = OpLoad %mat2v4float %mat9
// CHECK-NEXT: [[mat9_00:%\d+]] = OpCompositeExtract %float [[mat9]] 0 0
// CHECK-NEXT: [[mat9_01:%\d+]] = OpCompositeExtract %float [[mat9]] 0 1
// CHECK-NEXT: [[mat9_02:%\d+]] = OpCompositeExtract %float [[mat9]] 0 2
// CHECK-NEXT: [[mat9_03:%\d+]] = OpCompositeExtract %float [[mat9]] 0 3
// CHECK-NEXT: [[mat9_10:%\d+]] = OpCompositeExtract %float [[mat9]] 1 0
// CHECK-NEXT: [[mat9_11:%\d+]] = OpCompositeExtract %float [[mat9]] 1 1
// CHECK-NEXT: [[mat9_12:%\d+]] = OpCompositeExtract %float [[mat9]] 1 2
// CHECK-NEXT: [[mat9_13:%\d+]] = OpCompositeExtract %float [[mat9]] 1 3
// CHECK-NEXT: [[cc22:%\d+]] = OpCompositeConstruct %v4float [[mat9_00]] [[mat9_01]] [[mat9_02]] [[mat9_03]]
// CHECK-NEXT: [[cc23:%\d+]] = OpCompositeConstruct %v4float [[mat9_10]] [[mat9_11]] [[mat9_12]] [[mat9_13]]

// CHECK-NEXT: [[mat10:%\d+]] = OpLoad %v4float %mat10
// CHECK-NEXT: [[mat10_0:%\d+]] = OpCompositeExtract %float [[mat10]] 0
// CHECK-NEXT: [[mat10_1:%\d+]] = OpCompositeExtract %float [[mat10]] 1
// CHECK-NEXT: [[mat10_2:%\d+]] = OpCompositeExtract %float [[mat10]] 2
// CHECK-NEXT: [[mat10_3:%\d+]] = OpCompositeExtract %float [[mat10]] 3
// CHECK-NEXT: [[cc24:%\d+]] = OpCompositeConstruct %v4float [[mat10_0]] [[mat10_1]] [[mat10_2]] [[mat10_3]]

// CHECK-NEXT: [[cc25:%\d+]] = OpCompositeConstruct %mat4v4float [[cc21]] [[cc22]] [[cc23]] [[cc24]]
// CHECK-NEXT: OpStore %mat11 [[cc25]]
    float4x4 mat11 = {mat8, mat9, mat10};


    // Non-floating point matrices


    // Constructor
// CHECK:      [[cc00:%\d+]] = OpCompositeConstruct %v3int %int_1 %int_2 %int_3
// CHECK-NEXT: [[cc01:%\d+]] = OpCompositeConstruct %v3int %int_4 %int_5 %int_6
// CHECK-NEXT: [[cc02:%\d+]] = OpCompositeConstruct %_arr_v3int_uint_2 [[cc00]] [[cc01]]
// CHECK-NEXT: OpStore %imat1 [[cc02]]
    int2x3 imat1 = int2x3(1, 2, 3, 4, 5, 6);
    // All elements in a single {}
// CHECK-NEXT: [[cc03:%\d+]] = OpCompositeConstruct %v2int %int_1 %int_2
// CHECK-NEXT: [[cc04:%\d+]] = OpCompositeConstruct %v2int %int_3 %int_4
// CHECK-NEXT: [[cc05:%\d+]] = OpCompositeConstruct %v2int %int_5 %int_6
// CHECK-NEXT: [[cc06:%\d+]] = OpCompositeConstruct %_arr_v2int_uint_3 [[cc03]] [[cc04]] [[cc05]]
// CHECK-NEXT: OpStore %imat2 [[cc06]]
    int3x2 imat2 = {1, 2, 3, 4, 5, 6};
    // Each vector has its own {}
// CHECK-NEXT: [[cc07:%\d+]] = OpCompositeConstruct %v3int %int_1 %int_2 %int_3
// CHECK-NEXT: [[cc08:%\d+]] = OpCompositeConstruct %v3int %int_4 %int_5 %int_6
// CHECK-NEXT: [[cc09:%\d+]] = OpCompositeConstruct %_arr_v3int_uint_2 [[cc07]] [[cc08]]
// CHECK-NEXT: OpStore %imat3 [[cc09]]
    int2x3 imat3 = {{1, 2, 3}, {4, 5, 6}};
    // Wired & complicated {}s
// CHECK-NEXT: [[cc10:%\d+]] = OpCompositeConstruct %v2int %int_1 %int_2
// CHECK-NEXT: [[cc11:%\d+]] = OpCompositeConstruct %v2int %int_3 %int_4
// CHECK-NEXT: [[cc12:%\d+]] = OpCompositeConstruct %v2int %int_5 %int_6
// CHECK-NEXT: [[cc13:%\d+]] = OpCompositeConstruct %_arr_v2int_uint_3 [[cc10]] [[cc11]] [[cc12]]
// CHECK-NEXT: OpStore %imat4 [[cc13]]
    int3x2 imat4 = {{1}, {2, 3}, 4, {{5}, {{6}}}};

    int2 intVec2;
    int3 intVec3;
    int4 intVec4;

    // Mixed scalar and vector
// CHECK:         [[s:%\d+]] = OpLoad %int %intScalar
// CHECK-NEXT: [[vec1:%\d+]] = OpLoad %int %intVec1
// CHECK-NEXT: [[vec2:%\d+]] = OpLoad %v2int %intVec2
// CHECK-NEXT: [[ce00:%\d+]] = OpCompositeExtract %int [[vec2]] 0
// CHECK-NEXT: [[ce01:%\d+]] = OpCompositeExtract %int [[vec2]] 1
// CHECK-NEXT: [[cc14:%\d+]] = OpCompositeConstruct %v4int [[s]] [[vec1]] [[ce00]] [[ce01]]

// CHECK-NEXT: [[vec3:%\d+]] = OpLoad %v3int %intVec3
// CHECK-NEXT: [[ce02:%\d+]] = OpCompositeExtract %int [[vec3]] 0
// CHECK-NEXT: [[ce03:%\d+]] = OpCompositeExtract %int [[vec3]] 1
// CHECK-NEXT: [[ce04:%\d+]] = OpCompositeExtract %int [[vec3]] 2
// CHECK-NEXT:[[vec2a:%\d+]] = OpLoad %v2int %intVec2
// CHECK-NEXT: [[ce05:%\d+]] = OpCompositeExtract %int [[vec2a]] 0
// CHECK-NEXT: [[ce06:%\d+]] = OpCompositeExtract %int [[vec2a]] 1
// CHECK-NEXT: [[cc15:%\d+]] = OpCompositeConstruct %v4int [[ce02]] [[ce03]] [[ce04]] [[ce05]]

// CHECK-NEXT: [[cc16:%\d+]] = OpCompositeConstruct %v4int [[ce06]] %int_1 %int_2 %int_3

// CHECK-NEXT: [[vec4:%\d+]] = OpLoad %v4int %intVec4

// CHECK-NEXT: [[cc17:%\d+]] = OpCompositeConstruct %_arr_v4int_uint_4 [[cc14]] [[cc15]] [[cc16]] [[vec4]]
// CHECK-NEXT:  OpStore %imat5 [[cc17]]
    int4x4 imat5 = {intScalar, intVec1, intVec2, // [0]
                    intVec3,   intVec2,          // [1] + 1 scalar
                     int2(1, 2), 3,              // [2] - 1 scalar
                     intVec4                     // [3]
    };

    // From value of the same type
// CHECK-NEXT: [[imat5:%\d+]] = OpLoad %_arr_v4int_uint_4 %imat5
// CHECK-NEXT:                  OpStore %imat6 [[imat5]]
    int4x4 imat6 = int4x4(imat5);

    // Casting
    float floatScalar;
// CHECK:                      [[intVec1:%\d+]] = OpLoad %int %intVec1
// CHECK-NEXT:              [[uintScalar:%\d+]] = OpLoad %uint %uintScalar
// CHECK-NEXT:               [[intScalar:%\d+]] = OpBitcast %int [[uintScalar]]
// CHECK-NEXT:                [[uintVec2:%\d+]] = OpLoad %v2uint %uintVec2
// CHECK-NEXT:              [[uintVec2e0:%\d+]] = OpCompositeExtract %uint [[uintVec2]] 0
// CHECK-NEXT:              [[uintVec2e1:%\d+]] = OpCompositeExtract %uint [[uintVec2]] 1
// CHECK-NEXT:  [[convert_uintVec2e0_int:%\d+]] = OpBitcast %int [[uintVec2e0]]
// CHECK-NEXT:                [[imat7_r0:%\d+]] = OpCompositeConstruct %v3int [[intVec1]] [[intScalar]] [[convert_uintVec2e0_int]]
// CHECK-NEXT:  [[convert_uintVec2e1_int:%\d+]] = OpBitcast %int [[uintVec2e1]]
// CHECK-NEXT:             [[floatScalar:%\d+]] = OpLoad %float %floatScalar
// CHECK-NEXT: [[convert_floatScalar_int:%\d+]] = OpConvertFToS %int [[floatScalar]]
// CHECK-NEXT:              [[boolScalar:%\d+]] = OpLoad %bool %boolScalar
// CHECK-NEXT:  [[convert_boolScalar_int:%\d+]] = OpSelect %int [[boolScalar]] %int_1 %int_0
// CHECK-NEXT:                [[imat7_r1:%\d+]] = OpCompositeConstruct %v3int [[convert_uintVec2e1_int]] [[convert_floatScalar_int]] [[convert_boolScalar_int]]
// CHECK-NEXT:                  [[v3bool:%\d+]] = OpLoad %v3bool %boolVec3
// CHECK-NEXT:                [[imat7_r2:%\d+]] = OpSelect %v3int [[v3bool]] {{%\d+}} {{%\d+}}
// CHECK-NEXT:                         {{%\d+}} = OpCompositeConstruct %_arr_v3int_uint_3 [[imat7_r0]] [[imat7_r1]] [[imat7_r2]] 
    int3x3 imat7 = {intVec1, uintScalar, uintVec2, // [0] + 1 scalar
                    floatScalar, boolScalar,       // [1] - 1 scalar
                    boolVec3                       // [2]
    };

    // Decomposing matrices
    int2x2 imat8;
    int2x4 imat9;
    int4x1 imat10;
    // TODO: Optimization opportunity. We are extracting all elements in each
    // vector and then reconstructing the original vector. Optimally we should
    // extract vectors from matrices directly.

// CHECK:         [[imat8:%\d+]] = OpLoad %_arr_v2int_uint_2 %imat8
// CHECK-NEXT: [[imat8_00:%\d+]] = OpCompositeExtract %int [[imat8]] 0 0
// CHECK-NEXT: [[imat8_01:%\d+]] = OpCompositeExtract %int [[imat8]] 0 1
// CHECK-NEXT: [[imat8_10:%\d+]] = OpCompositeExtract %int [[imat8]] 1 0
// CHECK-NEXT: [[imat8_11:%\d+]] = OpCompositeExtract %int [[imat8]] 1 1
// CHECK-NEXT:     [[cc21:%\d+]] = OpCompositeConstruct %v4int [[imat8_00]] [[imat8_01]] [[imat8_10]] [[imat8_11]]

// CHECK-NEXT:    [[imat9:%\d+]] = OpLoad %_arr_v4int_uint_2 %imat9
// CHECK-NEXT: [[imat9_00:%\d+]] = OpCompositeExtract %int [[imat9]] 0 0
// CHECK-NEXT: [[imat9_01:%\d+]] = OpCompositeExtract %int [[imat9]] 0 1
// CHECK-NEXT: [[imat9_02:%\d+]] = OpCompositeExtract %int [[imat9]] 0 2
// CHECK-NEXT: [[imat9_03:%\d+]] = OpCompositeExtract %int [[imat9]] 0 3
// CHECK-NEXT: [[imat9_10:%\d+]] = OpCompositeExtract %int [[imat9]] 1 0
// CHECK-NEXT: [[imat9_11:%\d+]] = OpCompositeExtract %int [[imat9]] 1 1
// CHECK-NEXT: [[imat9_12:%\d+]] = OpCompositeExtract %int [[imat9]] 1 2
// CHECK-NEXT: [[imat9_13:%\d+]] = OpCompositeExtract %int [[imat9]] 1 3
// CHECK-NEXT:     [[cc22:%\d+]] = OpCompositeConstruct %v4int [[imat9_00]] [[imat9_01]] [[imat9_02]] [[imat9_03]]
// CHECK-NEXT:     [[cc23:%\d+]] = OpCompositeConstruct %v4int [[imat9_10]] [[imat9_11]] [[imat9_12]] [[imat9_13]]

// CHECK-NEXT: [[imat10:%\d+]] = OpLoad %v4int %imat10
// CHECK-NEXT: [[imat10_0:%\d+]] = OpCompositeExtract %int [[imat10]] 0
// CHECK-NEXT: [[imat10_1:%\d+]] = OpCompositeExtract %int [[imat10]] 1
// CHECK-NEXT: [[imat10_2:%\d+]] = OpCompositeExtract %int [[imat10]] 2
// CHECK-NEXT: [[imat10_3:%\d+]] = OpCompositeExtract %int [[imat10]] 3
// CHECK-NEXT: [[cc24:%\d+]] = OpCompositeConstruct %v4int [[imat10_0]] [[imat10_1]] [[imat10_2]] [[imat10_3]]

// CHECK-NEXT: [[cc25:%\d+]] = OpCompositeConstruct %_arr_v4int_uint_4 [[cc21]] [[cc22]] [[cc23]] [[cc24]]
// CHECK-NEXT: OpStore %imat11 [[cc25]]
    int4x4 imat11 = {imat8, imat9, imat10};

    // Boolean matrices
// CHECK:      [[cc00:%\d+]] = OpCompositeConstruct %v3bool %false %true %false
// CHECK-NEXT: [[cc01:%\d+]] = OpCompositeConstruct %v3bool %true %true %false
// CHECK-NEXT: [[cc02:%\d+]] = OpCompositeConstruct %_arr_v3bool_uint_2 [[cc00]] [[cc01]]
// CHECK-NEXT:                 OpStore %bmat1 [[cc02]]
    bool2x3 bmat1 = bool2x3(false, true, false, true, true, false);
    // All elements in a single {}
// CHECK-NEXT: [[cc03:%\d+]] = OpCompositeConstruct %v2bool %false %true
// CHECK-NEXT: [[cc04:%\d+]] = OpCompositeConstruct %v2bool %false %true
// CHECK-NEXT: [[cc05:%\d+]] = OpCompositeConstruct %v2bool %true %false
// CHECK-NEXT: [[cc06:%\d+]] = OpCompositeConstruct %_arr_v2bool_uint_3 [[cc03]] [[cc04]] [[cc05]]
// CHECK-NEXT:                 OpStore %bmat2 [[cc06]]
    bool3x2 bmat2 = {false, true, false, true, true, false};
    // Each vector has its own {}
// CHECK-NEXT: [[cc07:%\d+]] = OpCompositeConstruct %v3bool %false %true %false
// CHECK-NEXT: [[cc08:%\d+]] = OpCompositeConstruct %v3bool %true %true %false
// CHECK-NEXT: [[cc09:%\d+]] = OpCompositeConstruct %_arr_v3bool_uint_2 [[cc07]] [[cc08]]
// CHECK-NEXT:                 OpStore %bmat3 [[cc09]]
    bool2x3 bmat3 = {{false, true, false}, {true, true, false}};
    // Wired & complicated {}s
// CHECK-NEXT: [[cc10:%\d+]] = OpCompositeConstruct %v2bool %false %true
// CHECK-NEXT: [[cc11:%\d+]] = OpCompositeConstruct %v2bool %false %true
// CHECK-NEXT: [[cc12:%\d+]] = OpCompositeConstruct %v2bool %true %false
// CHECK-NEXT: [[cc13:%\d+]] = OpCompositeConstruct %_arr_v2bool_uint_3 [[cc10]] [[cc11]] [[cc12]]
// CHECK-NEXT:                 OpStore %bmat4 [[cc13]]
    bool3x2 bmat4 = {{false}, {true, false}, true, {{true}, {{false}}}};
}
