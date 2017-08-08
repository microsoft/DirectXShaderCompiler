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
}
