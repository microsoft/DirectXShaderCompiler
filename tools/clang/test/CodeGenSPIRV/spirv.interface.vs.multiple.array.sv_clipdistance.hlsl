// RUN: %dxc -T vs_6_0 -E main

struct VS_OUTPUT {
    float2 foo[4] : SV_ClipDistance0;
    float3 bar[2] : SV_ClipDistance1;
};

// CHECK:       %VS_OUTPUT = OpTypeStruct %_arr_v2float_uint_4 %_arr_v3float_uint_2
// CHECK: %gl_ClipDistance = OpVariable %_ptr_Output__arr_float_uint_14 Output

VS_OUTPUT main() {
// CHECK: [[VS_OUTPUT:%\d+]] = OpFunctionCall %VS_OUTPUT %src_main

// CHECK:  [[foo:%\d+]] = OpCompositeExtract %_arr_v2float_uint_4 [[VS_OUTPUT]] 0
// CHECK: [[offset_base:%\d+]] = OpIAdd %uint %uint_0 %uint_0

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_0
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[foo]] 0 0
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[foo]] 0 1
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset_base:%\d+]] = OpIAdd %uint %uint_0 %uint_2

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_0
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[foo]] 1 0
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[foo]] 1 1
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset_base:%\d+]] = OpIAdd %uint %uint_0 %uint_4

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_0
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[foo]] 2 0
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[foo]] 2 1
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset_base:%\d+]] = OpIAdd %uint %uint_0 %uint_6

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_0
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[foo]] 3 0
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[foo]] 3 1
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK:  [[bar:%\d+]] = OpCompositeExtract %_arr_v3float_uint_2 [[VS_OUTPUT]] 1
// CHECK: [[offset_base:%\d+]] = OpIAdd %uint %uint_8 %uint_0

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_0
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[bar]] 0 0
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[bar]] 0 1
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_2
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[bar]] 0 2

// CHECK: [[offset_base:%\d+]] = OpIAdd %uint %uint_8 %uint_3

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_0
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[bar]] 1 0
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[bar]] 1 1
// CHECK:                   OpStore [[ptr]] [[value]]

// CHECK: [[offset:%\d+]] = OpIAdd %uint [[offset_base]] %uint_2
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance [[offset]]
// CHECK:  [[value:%\d+]] = OpCompositeExtract %float [[bar]] 1 2

    return (VS_OUTPUT) 0;
}
